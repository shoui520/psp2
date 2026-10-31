/* paf's character classes (ScePafStdc). */
#ifndef PAF_STD_CTYPE_H
#define PAF_STD_CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

int sce_paf_look_ctype_table(int ch);
int sce_paf_tolower(int ch);
int sce_paf_private_towlower(int ch);
int sce_paf_toupper(int ch);

#ifdef __cplusplus
}
#endif

#endif
