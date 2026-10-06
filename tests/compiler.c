// Test if the compiler is valid

#if defined(__linux__)
#error "Target defined as linux, please change to a cross compilation"
#endif

#if !defined(__i386__)
#error "Wrong arch, please change the compiler"
#endif
