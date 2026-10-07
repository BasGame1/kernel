// Test if the compiler is valid

#if defined(__linux__)
#warning "Target defined as linux, please change to a cross compilation"
#endif

#if !defined(__i386__)
#warning "Wrong arch, please change the compiler"
#endif
