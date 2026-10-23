from __future__ import annotations

import unittest

from autobench_core.metrics import analyze_waveform
from autobench_core.signals import synthesize_waveform


class MetricsTests(unittest.TestCase):
    def test_frequency_thd_and_snr_are_reasonable(self) -> None:
        waveform = synthesize_waveform(
            1000.0,
            1.0,
            {"sample_rate_hz": 256000.0, "samples": 16384},
            {
                "frequency_error_ppm": 20.0,
                "noise_snr_db": 70.0,
                "harmonics_dbc": {2: -60.0, 3: -70.0},
                "seed": 1,
            },
            0,
        )
        result = analyze_waveform(waveform, 1000.0, 1.0)
        self.assertLess(abs(result.frequency_error_ppm - 20.0), 30.0)
        self.assertLess(result.thd_percent, 0.2)
        self.assertGreater(result.snr_db, 55.0)


if __name__ == "__main__":
    unittest.main()

