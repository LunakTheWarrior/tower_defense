#if defined(__cplusplus) || __STDC_VERSION__ < 199901L
#error "The C toolchain must compile C sources as C99 or newer."
#endif

int bastion_c99_probe(void) {
    const int values[3] = {[2] = 42};
    return values[2];
}
