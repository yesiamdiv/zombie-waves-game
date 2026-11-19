#ifndef ZW_CORE_COMPILER_H
#define ZW_CORE_COMPILER_H

/* Portable spellings of the compiler extensions this project relies on.
 *
 * W2: render.c used GCC's __attribute__((unused)) directly on three
 * file-scope declarations. MSVC has no such attribute and treats the token
 * sequence as a syntax error, which is a hard compile break, not warning
 * noise.
 *
 * ZW_UNUSED marks a declaration that may legitimately go unreferenced in some
 * build configurations. On GCC/Clang it expands to exactly the attribute that
 * was written before, so the Linux translation unit is unchanged. On MSVC it
 * expands to nothing: cl does not warn about an unreferenced file-scope
 * static at /W4 (C4505 is unreferenced local *functions*, and these are data
 * plus one static function that is in fact called), so dropping the marker
 * costs no diagnostic there.
 */
#if defined(__GNUC__) || defined(__clang__)
#  define ZW_UNUSED __attribute__((unused))
#else
#  define ZW_UNUSED
#endif

#endif /* ZW_CORE_COMPILER_H */
