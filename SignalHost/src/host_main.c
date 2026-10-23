/**
  ******************************************************************************
  * @file    host_main.c
  * @brief   SignalCore PC 仿真入口：CSV 输入、逐帧算法和 CSV/JSON 输出（UTF-8）。
  ******************************************************************************
  */

#include "signal_core_port.h"
#include "signal_fft.h"
#include "signal_joint_model.h"
#include "signal_preprocess.h"
#include "signal_vote.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef SIGNALHOST_CORE_DIR_TEXT
#define SIGNALHOST_CORE_DIR_TEXT "unknown"
#endif

#define HOST_LINE_CAPACITY     256U
#define HOST_MODE_CAPACITY     32U
#define HOST_NAME_CAPACITY     128U
#define HOST_FRAME_SAMPLES     SIGNAL_PREPROCESS_SAMPLE_COUNT

typedef struct
{
    uint16_t *samples;
    size_t sample_count;
    size_t sample_capacity;
    uint32_t sample_rate_hz;
    char analysis_mode[HOST_MODE_CAPACITY];
    char case_name[HOST_NAME_CAPACITY];
} HostInput;

typedef struct
{
    uint32_t frame_count;
    uint32_t preprocess_valid_frames;
    uint32_t fft_valid_frames;
    uint32_t joint_valid_frames;
    float peak_min_hz;
    float peak_max_hz;
    double peak_sum_hz;
    double peak_snr_sum_db;
    SignalJointResult last_joint;
    SignalVoteResult vote;
} HostSummary;

static void host_print_usage(const char *program)
{
    (void)fprintf(stderr,
                  "Usage: %s --input <csv> --output-csv <csv> "
                  "--output-json <json>\n",
                  program);
}

static bool host_parse_arguments(int argc,
                                 char **argv,
                                 const char **input_path,
                                 const char **csv_path,
                                 const char **json_path)
{
    *input_path = NULL;
    *csv_path = NULL;
    *json_path = NULL;
    for (int index = 1; index < argc; ++index)
    {
        if ((strcmp(argv[index], "--input") == 0) && (index + 1 < argc))
        {
            *input_path = argv[++index];
        }
        else if ((strcmp(argv[index], "--output-csv") == 0)
                 && (index + 1 < argc))
        {
            *csv_path = argv[++index];
        }
        else if ((strcmp(argv[index], "--output-json") == 0)
                 && (index + 1 < argc))
        {
            *json_path = argv[++index];
        }
        else
        {
            return false;
        }
    }
    return (*input_path != NULL) && (*csv_path != NULL) && (*json_path != NULL);
}

static char *host_trim(char *text)
{
    while ((*text != '\0') && isspace((unsigned char)*text))
    {
        ++text;
    }
    char *end = text + strlen(text);
    while ((end > text) && isspace((unsigned char)end[-1]))
    {
        --end;
    }
    *end = '\0';
    return text;
}

static void host_copy_text(char *destination,
                           size_t capacity,
                           const char *source)
{
    if ((destination == NULL) || (capacity == 0U))
    {
        return;
    }
    (void)snprintf(destination, capacity, "%s", source != NULL ? source : "");
}

static bool host_append_sample(HostInput *input, uint16_t sample)
{
    if (input->sample_count == input->sample_capacity)
    {
        size_t next_capacity = (input->sample_capacity == 0U)
            ? (size_t)HOST_FRAME_SAMPLES
            : input->sample_capacity * 2U;
        uint16_t *next = (uint16_t *)realloc(
            input->samples, next_capacity * sizeof(uint16_t));
        if (next == NULL)
        {
            return false;
        }
        input->samples = next;
        input->sample_capacity = next_capacity;
    }
    input->samples[input->sample_count++] = sample;
    return true;
}

static bool host_parse_header(HostInput *input, char *line)
{
    char *text = host_trim(line + 1);
    char *equals = strchr(text, '=');
    if (equals == NULL)
    {
        return true;
    }
    *equals = '\0';
    char *key = host_trim(text);
    char *value = host_trim(equals + 1);
    if (strcmp(key, "sample_rate_hz") == 0)
    {
        char *end = NULL;
        unsigned long parsed = strtoul(value, &end, 10);
        if ((end == value) || (*host_trim(end) != '\0')
            || (parsed == 0UL) || (parsed > UINT32_MAX))
        {
            return false;
        }
        input->sample_rate_hz = (uint32_t)parsed;
    }
    else if (strcmp(key, "analysis_mode") == 0)
    {
        host_copy_text(input->analysis_mode,
                       sizeof(input->analysis_mode), value);
    }
    else if (strcmp(key, "case_name") == 0)
    {
        host_copy_text(input->case_name, sizeof(input->case_name), value);
    }
    return true;
}

static bool host_load_input(const char *path, HostInput *input)
{
    memset(input, 0, sizeof(*input));
    host_copy_text(input->analysis_mode, sizeof(input->analysis_mode), "dual");
    host_copy_text(input->case_name, sizeof(input->case_name), "unnamed");

    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        (void)fprintf(stderr, "ERROR: cannot open input CSV: %s\n", path);
        return false;
    }

    bool ok = true;
    char line[HOST_LINE_CAPACITY];
    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        char *text = host_trim(line);
        if (*text == '\0')
        {
            continue;
        }
        if (*text == '#')
        {
            if (!host_parse_header(input, text))
            {
                ok = false;
                break;
            }
            continue;
        }

        errno = 0;
        char *end = NULL;
        long parsed = strtol(text, &end, 10);
        if ((errno != 0) || (end == text) || (*host_trim(end) != '\0')
            || (parsed < 0L) || (parsed > 4095L)
            || !host_append_sample(input, (uint16_t)parsed))
        {
            ok = false;
            break;
        }
    }

    if (ferror(file) != 0)
    {
        ok = false;
    }
    (void)fclose(file);

    if (!ok || (input->sample_rate_hz == 0U)
        || (input->sample_count == 0U)
        || ((input->sample_count % HOST_FRAME_SAMPLES) != 0U))
    {
        (void)fprintf(stderr,
                      "ERROR: invalid input CSV; rate=%u samples=%zu "
                      "(must be a multiple of %u)\n",
                      input->sample_rate_hz,
                      input->sample_count,
                      HOST_FRAME_SAMPLES);
        free(input->samples);
        memset(input, 0, sizeof(*input));
        return false;
    }
    return true;
}

static uint16_t host_raw_to_code(uint16_t raw_sample)
{
    return raw_sample;
}

static uint32_t host_timer_count(void)
{
    return (uint32_t)clock();
}

static uint32_t host_timer_frequency_hz(void)
{
    return (uint32_t)CLOCKS_PER_SEC;
}

static float host_selected_snr(const SignalFftResult *result)
{
    float best = -INFINITY;
    for (uint32_t index = 0U;
         index < result->selected_candidate_count;
         ++index)
    {
        if (result->selected[index].snr_db > best)
        {
            best = result->selected[index].snr_db;
        }
    }
    return isfinite(best) ? best : 0.0f;
}

static void host_load_fft_config(uint32_t sample_rate_hz,
                                 SignalFftConfig *config)
{
    SignalFft_LoadDefaultConfig(config, sample_rate_hz);
    uint32_t resolution_hz = (sample_rate_hz + HOST_FRAME_SAMPLES - 1U)
                             / HOST_FRAME_SAMPLES;
    uint32_t nyquist_guard_hz = (sample_rate_hz / 2U)
                                 - (2U * resolution_hz);
    config->min_frequency_hz = 500U;
    config->max_frequency_hz = nyquist_guard_hz < 50000U
        ? nyquist_guard_hz
        : 50000U;
    config->candidate_step_hz = resolution_hz > 500U
        ? resolution_hz
        : 500U;
    config->minimum_snr_db = 6.0f;
    config->minimum_amplitude_code = 4.0f;
    config->max_selected_candidates = APP_ANALYSIS_MAX_SELECTED_CANDIDATES;
}

static bool host_process(const HostInput *input,
                         const char *csv_path,
                         HostSummary *summary)
{
    FILE *csv = fopen(csv_path, "wb");
    if (csv == NULL)
    {
        (void)fprintf(stderr, "ERROR: cannot create result CSV: %s\n", csv_path);
        return false;
    }

    (void)fprintf(csv,
        "frame_index,start_sample,preprocess_valid,fft_valid,joint_valid,"
        "peak_frequency_hz,peak_amplitude_code,selected_snr_db,"
        "component_0_frequency_hz,component_0_amplitude_code,component_0_type,"
        "component_1_frequency_hz,component_1_amplitude_code,component_1_type,"
        "joint_residual,joint_confidence\n");

    memset(summary, 0, sizeof(*summary));
    summary->peak_min_hz = INFINITY;
    summary->peak_max_hz = -INFINITY;

    SignalPreprocess_Init();
    SignalPreprocess_ResetDiagnostics();
    SignalFft_ResetDiagnostics();
    if (!SignalFft_Init())
    {
        (void)fclose(csv);
        (void)fputs("ERROR: SignalFft_Init failed\n", stderr);
        return false;
    }

    bool peak_only = strcmp(input->analysis_mode, "peak") == 0;
    SignalVote_Begin();
    size_t frame_count = input->sample_count / HOST_FRAME_SAMPLES;
    summary->frame_count = (uint32_t)frame_count;

    for (size_t frame = 0U; frame < frame_count; ++frame)
    {
        const uint16_t *raw = input->samples + (frame * HOST_FRAME_SAMPLES);
        SignalPreprocessInfo preprocess = {0};
        SignalFftResult fft = {0};
        SignalJointResult joint = {0};
        uint32_t frame_start = SignalCorePort_TimerCount();
        bool preprocess_ok = SignalPreprocess_Run(
            raw,
            HOST_FRAME_SAMPLES,
            g_signal_preprocess_windowed,
            g_signal_preprocess_unwindowed,
            &preprocess);
        if (preprocess_ok)
        {
            ++summary->preprocess_valid_frames;
        }
        SignalPreprocess_RecordDiagnostics(
            &preprocess,
            SignalCorePort_ElapsedUs(frame_start, SignalCorePort_TimerCount()));

        bool fft_ok = false;
        float fft_input[HOST_FRAME_SAMPLES];
        if (preprocess_ok)
        {
            memcpy(fft_input,
                   g_signal_preprocess_windowed,
                   sizeof(fft_input));
            SignalFftConfig config;
            host_load_fft_config(input->sample_rate_hz, &config);
            uint32_t fft_start = SignalCorePort_TimerCount();
            fft_ok = SignalFft_Run(fft_input, &config, &fft);
            SignalFft_RecordDiagnostics(
                fft_ok,
                SignalCorePort_ElapsedUs(fft_start, SignalCorePort_TimerCount()));
        }

        if (isfinite(fft.full_spectrum_peak_frequency_hz)
            && (fft.full_spectrum_peak_bin > 0U))
        {
            ++summary->fft_valid_frames;
            float peak = fft.full_spectrum_peak_frequency_hz;
            if (peak < summary->peak_min_hz)
            {
                summary->peak_min_hz = peak;
            }
            if (peak > summary->peak_max_hz)
            {
                summary->peak_max_hz = peak;
            }
            summary->peak_sum_hz += peak;
            summary->peak_snr_sum_db += host_selected_snr(&fft);
        }

        bool joint_ok = false;
        if (!peak_only && fft_ok && (fft.selected_candidate_count >= 2U))
        {
            uint32_t joint_start = SignalCorePort_TimerCount();
            joint_ok = SignalJoint_Analyze(g_signal_preprocess_unwindowed,
                                           HOST_FRAME_SAMPLES,
                                           &fft,
                                           &joint);
            SignalJoint_PublishResult(
                &joint,
                SignalCorePort_ElapsedUs(joint_start,
                                         SignalCorePort_TimerCount()));
        }

        if (!peak_only)
        {
            if (joint_ok)
            {
                ++summary->joint_valid_frames;
                summary->last_joint = joint;
                SignalVote_AddFrame(&joint,
                    SignalCorePort_ElapsedUs(frame_start,
                                             SignalCorePort_TimerCount()));
            }
            else if (!preprocess_ok)
            {
                SignalVote_AddInvalidFrame(
                    preprocess.clip_ratio > SIGNAL_PREPROCESS_MAX_CLIP_RATIO
                        ? SIGNAL_DETECT_ERR_CLIPPING
                        : SIGNAL_DETECT_ERR_LOW_AMPLITUDE);
            }
            else
            {
                SignalVote_AddInvalidFrame(SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS);
            }
        }

        (void)fprintf(csv,
            "%zu,%zu,%u,%u,%u,%.6f,%.6f,%.6f,"
            "%.6f,%.6f,%s,%.6f,%.6f,%s,%.8f,%.8f\n",
            frame,
            frame * HOST_FRAME_SAMPLES,
            preprocess_ok ? 1U : 0U,
            fft_ok ? 1U : 0U,
            joint_ok ? 1U : 0U,
            (double)fft.full_spectrum_peak_frequency_hz,
            (double)fft.full_spectrum_peak_amplitude_code,
            (double)host_selected_snr(&fft),
            (double)joint.components[0].frequency_hz,
            (double)joint.components[0].amplitude_code,
            SignalHarmonic_WaveTypeString(joint.components[0].type),
            (double)joint.components[1].frequency_hz,
            (double)joint.components[1].amplitude_code,
            SignalHarmonic_WaveTypeString(joint.components[1].type),
            (double)joint.best_residual,
            (double)joint.confidence);
    }

    if (!peak_only)
    {
        (void)SignalVote_Finalize(0U, 0U, 0U, &summary->vote);
    }
    (void)fclose(csv);
    return true;
}

static void host_json_string(FILE *file, const char *text)
{
    (void)fputc('"', file);
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor != '\0'; ++cursor)
    {
        if ((*cursor == '"') || (*cursor == '\\'))
        {
            (void)fputc('\\', file);
            (void)fputc((int)*cursor, file);
        }
        else if (*cursor == '\n')
        {
            (void)fputs("\\n", file);
        }
        else
        {
            (void)fputc((int)*cursor, file);
        }
    }
    (void)fputc('"', file);
}

static void host_sort_components(SignalVoteComponent components[2])
{
    if (components[0].frequency_hz > components[1].frequency_hz)
    {
        SignalVoteComponent temporary = components[0];
        components[0] = components[1];
        components[1] = temporary;
    }
}

static bool host_write_json(const char *path,
                            const HostInput *input,
                            const HostSummary *summary)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL)
    {
        (void)fprintf(stderr, "ERROR: cannot create result JSON: %s\n", path);
        return false;
    }

    bool peak_only = strcmp(input->analysis_mode, "peak") == 0;
    bool valid = peak_only
        ? (summary->fft_valid_frames == summary->frame_count)
        : summary->vote.valid;
    float mean_peak = summary->fft_valid_frames > 0U
        ? (float)(summary->peak_sum_hz / summary->fft_valid_frames)
        : 0.0f;
    float mean_snr = summary->fft_valid_frames > 0U
        ? (float)(summary->peak_snr_sum_db / summary->fft_valid_frames)
        : 0.0f;
    SignalVoteComponent components[2] = {
        summary->vote.components[0], summary->vote.components[1]};
    host_sort_components(components);

    (void)fputs("{\n  \"case_name\": ", file);
    host_json_string(file, input->case_name);
    (void)fputs(",\n  \"analysis_mode\": ", file);
    host_json_string(file, input->analysis_mode);
    (void)fputs(",\n  \"signal_core_dir\": ", file);
    host_json_string(file, SIGNALHOST_CORE_DIR_TEXT);
    (void)fprintf(file,
        ",\n  \"sample_rate_hz\": %u,\n"
        "  \"frame_samples\": %u,\n"
        "  \"frame_count\": %u,\n"
        "  \"valid\": %s,\n"
        "  \"preprocess_valid_frames\": %u,\n"
        "  \"fft_valid_frames\": %u,\n"
        "  \"joint_valid_frames\": %u,\n"
        "  \"peak_track\": {\n"
        "    \"minimum_hz\": %.6f,\n"
        "    \"maximum_hz\": %.6f,\n"
        "    \"mean_hz\": %.6f,\n"
        "    \"mean_selected_snr_db\": %.6f\n"
        "  },\n",
        input->sample_rate_hz,
        HOST_FRAME_SAMPLES,
        summary->frame_count,
        valid ? "true" : "false",
        summary->preprocess_valid_frames,
        summary->fft_valid_frames,
        summary->joint_valid_frames,
        isfinite(summary->peak_min_hz) ? (double)summary->peak_min_hz : 0.0,
        isfinite(summary->peak_max_hz) ? (double)summary->peak_max_hz : 0.0,
        (double)mean_peak,
        (double)mean_snr);

    (void)fprintf(file,
        "  \"vote\": {\n"
        "    \"valid\": %s,\n"
        "    \"error\": \"%s\",\n"
        "    \"valid_frames\": %u,\n"
        "    \"invalid_frames\": %u,\n"
        "    \"median_residual\": %.8f,\n"
        "    \"median_model_confidence\": %.8f,\n"
        "    \"total_confidence\": %.8f\n"
        "  },\n"
        "  \"components\": [\n",
        summary->vote.valid ? "true" : "false",
        SignalVote_ErrorString(summary->vote.error),
        summary->vote.valid_frames,
        summary->vote.invalid_frames,
        (double)summary->vote.median_residual,
        (double)summary->vote.median_model_confidence,
        (double)summary->vote.total_confidence);

    for (uint32_t index = 0U; index < 2U; ++index)
    {
        const SignalVoteComponent *component = &components[index];
        (void)fprintf(file,
            "    {\"frequency_hz\": %.6f, \"amplitude_code\": %.6f, "
            "\"phase_rad\": %.8f, \"type\": \"%s\", "
            "\"frequency_votes\": %u, \"type_votes\": %u}%s\n",
            (double)component->frequency_hz,
            (double)component->amplitude_code,
            (double)component->phase_rad,
            SignalHarmonic_WaveTypeString(component->type),
            component->frequency_votes,
            component->type_votes,
            index == 0U ? "," : "");
    }
    (void)fputs("  ]\n}\n", file);
    (void)fclose(file);
    return true;
}

int main(int argc, char **argv)
{
    const char *input_path;
    const char *csv_path;
    const char *json_path;
    if (!host_parse_arguments(argc, argv, &input_path, &csv_path, &json_path))
    {
        host_print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    HostInput input;
    if (!host_load_input(input_path, &input))
    {
        return EXIT_FAILURE;
    }

    const SignalCorePort port = {
        host_raw_to_code,
        host_timer_count,
        host_timer_frequency_hz,
    };
    if (!SignalCorePort_Configure(&port))
    {
        (void)fputs("ERROR: SignalCorePort_Configure failed\n", stderr);
        free(input.samples);
        return EXIT_FAILURE;
    }

    HostSummary summary;
    bool ok = host_process(&input, csv_path, &summary)
              && host_write_json(json_path, &input, &summary);
    if (ok)
    {
        (void)printf(
            "SignalHost: case=%s mode=%s frames=%u preprocess=%u fft=%u "
            "joint=%u vote_valid=%s confidence=%.6f core=%s\n",
            input.case_name,
            input.analysis_mode,
            summary.frame_count,
            summary.preprocess_valid_frames,
            summary.fft_valid_frames,
            summary.joint_valid_frames,
            summary.vote.valid ? "true" : "false",
            (double)summary.vote.total_confidence,
            SIGNALHOST_CORE_DIR_TEXT);
    }
    free(input.samples);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

