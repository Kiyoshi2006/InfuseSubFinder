// SFMatch - khop ten video voi ten file sub (C thuan, khong phu thuoc gi)
#ifndef SFMATCH_H
#define SFMATCH_H

#ifdef __cplusplus
extern "C" {
#endif

// Them alias: moi lan xuat hien cum `from` (khong phan biet hoa thuong) trong ten
// se duoc doi thanh `to` truoc khi so khop. Vi du: danmachi -> "dungeon ni deai wo ..."
void sf_clear_aliases(void);
void sf_add_alias(const char *from, const char *to);

// Cham diem 0..100 giua (video) va (sub). *Name la ten file (co hoac khong co duoi),
// *Folder la ten thu muc cha (co the NULL/rong). 0 = khong khop.
int sf_score(const char *videoName, const char *videoFolder,
             const char *subName, const char *subFolder);

// Doan ngon ngu tu ten file sub (vd "Show.S01E02.vi.srt" -> "Vietnamese"). NULL neu khong ro.
const char *sf_language_from_name(const char *subName);

// 1 neu `ext` (khong co dau cham, khong phan biet hoa thuong) la duoi file sub ho tro.
int sf_is_subtitle_ext(const char *ext);

// Phan tich ten: lay mua/tap (-1 neu khong co). Tra ve 1 neu co tap.
int sf_parse_se(const char *name, int *season, int *episode);

// Mo ta cach phan tich ten (de chan doan), vd "S2 E1 Y0 [dungeon deai motomeru ...]".
void sf_describe(const char *name, char *out, unsigned long cap);

#ifdef __cplusplus
}
#endif

#endif
