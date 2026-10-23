#ifndef SIGNALHOST_COMPILER_COMPAT_H_
#define SIGNALHOST_COMPILER_COMPAT_H_

/* Host 构建不需要 MCU 链接段属性，统一剥离以兼容 MSVC/clang。 */
#ifdef __attribute__
#undef __attribute__
#endif
#define __attribute__(x)

#endif /* SIGNALHOST_COMPILER_COMPAT_H_ */

