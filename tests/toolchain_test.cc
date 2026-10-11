#include <expected>
#include <iostream>

#if !defined(__GNUC__) || __GNUC__ < 13 || defined(__clang__)
#error "The registered toolchain must use GCC 13 or newer."
#endif
#if !defined(__x86_64__) || !defined(__linux__)
#error "The registered toolchain must target Linux x86_64."
#endif
static_assert(__cplusplus >= 202100L, "The toolchain must enable C++23.");
static_assert(sizeof(void*) == 8, "The toolchain must use the 64-bit ABI.");

extern "C" int bastion_c99_probe(void);

int main() {
    const std::expected<int, int> answer = bastion_c99_probe();
    if (!answer || answer.value_or(0) * 2 != 84) return 1;
    std::cout << "GCC " << __VERSION__ << ": C99 + C++23, x86_64 Linux, mixed-language linking OK.\n";
}
