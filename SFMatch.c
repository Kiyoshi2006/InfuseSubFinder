// SFMatch - khop ten video voi ten file sub. Xem SFMatch.h.
#include "SFMatch.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF_MAX_TOK 64
#define SF_TOK_LEN 32
#define SF_MAX_TITLE 24
#define SF_MAX_ALIASES 64
#define SF_ALIAS_LEN 160

typedef struct {
    int season;   // -1 = chua biet
    int episode;  // -1 = chua biet
    int year;     // 0 = chua biet
    int ntitle;
    char title[SF_MAX_TITLE][SF_TOK_LEN];  // token tieu de (da bo nhieu, stopword)
    int nfull;
    char full[SF_MAX_TOK][SF_TOK_LEN];     // moi token tru token ngon ngu/duoi file
} SFInfo;

// ---------------------------------------------------------------- bang tu

static const char *HARD_NOISE[] = {
    "480p", "540p", "576p", "720p", "1080p", "1080i", "1440p", "2160p", "4320p",
    "4k", "uhd", "fhd", "bluray", "blu", "bdrip", "brrip", "bdremux", "remux",
    "webrip", "webdl", "web", "dl", "hdtv", "dvdrip", "dvd", "hdrip", "amzn", "nf",
    "dsnp", "hmax", "atvp", "hulu", "x264", "x265", "h264", "h265", "hevc", "avc",
    "xvid", "10bit", "8bit", "aac", "ac3", "eac3", "dts", "flac", "ddp5", "ddp",
    "dd5", "atmos", "truehd", "hdr", "hdr10", "dv", "hybrid", "264", "265", NULL};

static const char *SOFT_NOISE[] = {
    "vi", "vie", "vn", "vietsub", "vietnamese", "eng", "en", "english", "engsub",
    "jp", "jpn", "ja", "japanese", "zh", "chs", "cht", "chi", "zho", "chinese",
    "ko", "kor", "korean", "fr", "fre", "fra", "french", "de", "ger", "deu", "german",
    "es", "spa", "spanish", "pt", "por", "portuguese", "ru", "rus", "russian",
    "th", "tha", "thai", "id", "ind", "indonesian", "sub", "subs", "subtitle",
    "subtitles", "softsub", "hardsub", "srt", "ass", "ssa", "vtt", "mkv", "mp4",
    "avi", "proper", "repack", "extended", "uncut", "remastered", "multi", "dual",
    "internal", "complete", "final", "v1", "v2", "v3", "forced", "sdh", "cc", NULL};

static const char *STOPWORDS[] = {
    "the", "a", "an", "of", "and", "no", "wa", "ga", "wo", "ni", "to", "de", "na",
    "ka", "is", "in", "on", "at", "for", "season", NULL};

static const char *KNOWN_EXT[] = {
    "mkv", "mp4", "avi", "mov", "wmv", "flv", "ts", "m2ts", "webm", "m4v", "mpg",
    "mpeg", "srt", "ass", "ssa", "vtt", "sub", "idx", "sup", NULL};

typedef struct { const char *tag; const char *lang; } LangMap;
static const LangMap LANGS[] = {
    {"vi", "Vietnamese"}, {"vie", "Vietnamese"}, {"vn", "Vietnamese"},
    {"vietsub", "Vietnamese"}, {"vietnamese", "Vietnamese"},
    {"en", "English"}, {"eng", "English"}, {"english", "English"}, {"engsub", "English"},
    {"ja", "Japanese"}, {"jp", "Japanese"}, {"jpn", "Japanese"}, {"japanese", "Japanese"},
    {"zh", "Chinese"}, {"chs", "Chinese"}, {"cht", "Chinese"}, {"chi", "Chinese"},
    {"zho", "Chinese"}, {"chinese", "Chinese"},
    {"ko", "Korean"}, {"kor", "Korean"}, {"korean", "Korean"},
    {"fr", "French"}, {"fre", "French"}, {"fra", "French"}, {"french", "French"},
    {"de", "German"}, {"ger", "German"}, {"deu", "German"}, {"german", "German"},
    {"es", "Spanish"}, {"spa", "Spanish"}, {"spanish", "Spanish"},
    {"pt", "Portuguese"}, {"por", "Portuguese"}, {"portuguese", "Portuguese"},
    {"ru", "Russian"}, {"rus", "Russian"}, {"russian", "Russian"},
    {"th", "Thai"}, {"tha", "Thai"}, {"thai", "Thai"},
    {"id", "Indonesian"}, {"ind", "Indonesian"}, {"indonesian", "Indonesian"},
    {NULL, NULL}};

static int in_list(const char *const *list, const char *t) {
    for (int i = 0; list[i]; i++)
        if (strcmp(list[i], t) == 0) return 1;
    return 0;
}

// ---------------------------------------------------------------- alias

static char g_alias_from[SF_MAX_ALIASES][SF_ALIAS_LEN];
static char g_alias_to[SF_MAX_ALIASES][SF_ALIAS_LEN];
static int g_alias_n = 0;

static void normalize_phrase(const char *in, char *out, size_t cap) {
    size_t o = 0;
    int lastSpace = 1;
    if (cap == 0) return;
    for (const unsigned char *p = (const unsigned char *)(in ? in : ""); *p && o + 1 < cap; p++) {
        unsigned char c = *p;
        if (c < 0x80) {
            if (isalnum(c)) {
                out[o++] = (char)tolower(c);
                lastSpace = 0;
            } else if (!lastSpace) {
                out[o++] = ' ';
                lastSpace = 1;
            }
        } else {  // giu nguyen byte UTF-8 de khong lam hong chu co dau
            out[o++] = (char)c;
            lastSpace = 0;
        }
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = 0;
}

void sf_clear_aliases(void) { g_alias_n = 0; }

void sf_add_alias(const char *from, const char *to) {
    if (!from || !to || g_alias_n >= SF_MAX_ALIASES) return;
    char f[SF_ALIAS_LEN], t[SF_ALIAS_LEN];
    normalize_phrase(from, f, sizeof f);
    normalize_phrase(to, t, sizeof t);
    if (!f[0] || !t[0] || strcmp(f, t) == 0) return;
    strcpy(g_alias_from[g_alias_n], f);
    strcpy(g_alias_to[g_alias_n], t);
    g_alias_n++;
}

// Thay cum tu nguyen ven (bao quanh boi khoang trang / dau-cuoi chuoi).
static void apply_aliases(char *s, size_t cap) {
    for (int i = 0; i < g_alias_n; i++) {
        const char *from = g_alias_from[i], *to = g_alias_to[i];
        size_t fl = strlen(from), tl = strlen(to);
        char tmp[1024];
        size_t o = 0;
        const char *p = s;
        int changed = 0;
        while (*p && o + 1 < sizeof tmp) {
            const char *hit = strstr(p, from);
            while (hit) {
                int leftOk = (hit == s) || hit[-1] == ' ';
                int rightOk = hit[fl] == 0 || hit[fl] == ' ';
                if (leftOk && rightOk) break;
                hit = strstr(hit + 1, from);
            }
            if (!hit) break;
            size_t pre = (size_t)(hit - p);
            if (o + pre + tl + 1 >= sizeof tmp) break;
            memcpy(tmp + o, p, pre);
            o += pre;
            memcpy(tmp + o, to, tl);
            o += tl;
            p = hit + fl;
            changed = 1;
        }
        if (changed) {
            size_t rest = strlen(p);
            if (o + rest + 1 < sizeof tmp) {
                memcpy(tmp + o, p, rest);
                o += rest;
                tmp[o] = 0;
                if (o + 1 <= cap) memcpy(s, tmp, o + 1);
            }
        }
    }
}

// ---------------------------------------------------------------- parse

static void strip_ext(char *s) {
    char *dot = strrchr(s, '.');
    if (!dot || !dot[1]) return;
    size_t el = strlen(dot + 1);
    if (el > 5) return;
    char ext[8];
    for (size_t i = 0; i < el; i++) ext[i] = (char)tolower((unsigned char)dot[1 + i]);
    ext[el] = 0;
    if (in_list(KNOWN_EXT, ext)) *dot = 0;
}

// Bo noi dung trong [] {} (); lay nam neu (dddd).
static void strip_brackets(const char *in, char *out, size_t cap, int *year) {
    size_t o = 0;
    for (const char *p = in; *p && o + 2 < cap;) {
        char c = *p, close = 0;
        if (c == '[') close = ']';
        else if (c == '{') close = '}';
        else if (c == '(') close = ')';
        if (close) {
            const char *q = strchr(p + 1, close);
            if (q) {
                if (c == '(' && q - p - 1 == 4 && isdigit((unsigned char)p[1]) &&
                    isdigit((unsigned char)p[2]) && isdigit((unsigned char)p[3]) &&
                    isdigit((unsigned char)p[4])) {
                    int y = atoi(p + 1);
                    if (y >= 1900 && y <= 2099 && year) *year = y;
                }
                out[o++] = ' ';
                p = q + 1;
                continue;
            }
        }
        out[o++] = c;
        p++;
    }
    out[o] = 0;
}

static int all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

// s01e02, s1e2, s01e02e03 ...
static int parse_sxxexx(const char *t, int *season, int *ep) {
    if (t[0] != 's') return 0;
    const char *p = t + 1;
    int sv = 0, nd = 0;
    while (isdigit((unsigned char)*p) && nd < 3) { sv = sv * 10 + (*p - '0'); p++; nd++; }
    if (!nd || *p != 'e') return 0;
    p++;
    int ev = 0;
    nd = 0;
    while (isdigit((unsigned char)*p) && nd < 4) { ev = ev * 10 + (*p - '0'); p++; nd++; }
    if (!nd) return 0;
    *season = sv;
    *ep = ev;
    return 1;
}

// 1x02, 01x002 (khong nhan 1920x1080)
static int parse_nxnn(const char *t, int *season, int *ep) {
    const char *p = t;
    int sv = 0, nd = 0;
    while (isdigit((unsigned char)*p) && nd < 3) { sv = sv * 10 + (*p - '0'); p++; nd++; }
    if (nd < 1 || nd > 2 || *p != 'x') return 0;
    p++;
    int ev = 0;
    nd = 0;
    while (isdigit((unsigned char)*p) && nd < 4) { ev = ev * 10 + (*p - '0'); p++; nd++; }
    if (nd < 2 || nd > 3 || *p != 0) return 0;
    *season = sv;
    *ep = ev;
    return 1;
}

// e02, ep02, ep 02 (so nam o token ke tiep, xu ly o ngoai)
static int parse_exx(const char *t, int *ep) {
    const char *p = t;
    if (p[0] == 'e' && p[1] == 'p') p += 2;
    else if (p[0] == 'e') p += 1;
    else return 0;
    if (!*p || !all_digits(p) || strlen(p) > 4) return 0;
    *ep = atoi(p);
    return 1;
}

static int parse_sxx(const char *t, int *season) {
    if (t[0] != 's' || !t[1] || !all_digits(t + 1) || strlen(t + 1) > 2) return 0;
    *season = atoi(t + 1);
    return 1;
}

static int is_year_token(const char *t) {
    if (strlen(t) != 4 || !all_digits(t)) return 0;
    int y = atoi(t);
    return y >= 1900 && y <= 2099;
}

// " - 25" kieu fansub
static int find_dash_episode(const char *s) {
    for (const char *p = s; (p = strstr(p, " - ")) != NULL; p += 3) {
        const char *q = p + 3;
        int n = 0, v = 0;
        while (isdigit((unsigned char)*q) && n < 4) { v = v * 10 + (*q - '0'); q++; n++; }
        if (n >= 1 && n <= 3 && (*q == 0 || *q == ' ' || *q == 'v' || *q == '_' || *q == '.')) return v;
    }
    return -1;
}

static void parse_info(const char *name, SFInfo *info) {
    memset(info, 0, sizeof *info);
    info->season = -1;
    info->episode = -1;
    if (!name || !*name) return;

    char a[512], b[512], c[1024];
    snprintf(a, sizeof a, "%s", name);
    strip_ext(a);
    int year = 0;
    strip_brackets(a, b, sizeof b, &year);
    int dashEp = find_dash_episode(b);
    normalize_phrase(b, c, sizeof c);
    apply_aliases(c, sizeof c);

    // tach token
    char toks[SF_MAX_TOK][SF_TOK_LEN];
    int n = 0;
    for (char *p = c; *p && n < SF_MAX_TOK;) {
        while (*p == ' ') p++;
        if (!*p) break;
        int l = 0;
        while (*p && *p != ' ') {
            if (l < SF_TOK_LEN - 1) toks[n][l++] = *p;
            p++;
        }
        toks[n][l] = 0;
        n++;
    }

    int epIdx = -1, yearIdx = -1, firstHard = -1;
    for (int i = 0; i < n; i++) {
        int s = 0, e = 0;
        const char *t = toks[i];
        if (firstHard < 0 && i > 0 && in_list(HARD_NOISE, t)) firstHard = i;
        if (info->episode < 0) {
            if (parse_sxxexx(t, &s, &e) || parse_nxnn(t, &s, &e)) {
                info->season = s; info->episode = e; epIdx = i; continue;
            }
            if (parse_exx(t, &e)) {
                info->episode = e; epIdx = i; continue;
            }
            if ((strcmp(t, "episode") == 0 || strcmp(t, "ep") == 0) && i + 1 < n && all_digits(toks[i + 1]) &&
                strlen(toks[i + 1]) <= 4) {
                info->episode = atoi(toks[i + 1]); epIdx = i; continue;
            }
        }
        if (info->season < 0) {
            if (parse_sxx(t, &s)) { info->season = s; if (epIdx < 0) epIdx = i; continue; }
            if (strcmp(t, "season") == 0 && i + 1 < n && all_digits(toks[i + 1]) && strlen(toks[i + 1]) <= 2) {
                info->season = atoi(toks[i + 1]); if (epIdx < 0) epIdx = i; continue;
            }
        }
        if (yearIdx < 0 && i > 0 && is_year_token(t)) yearIdx = i;
    }

    if (info->episode < 0 && dashEp >= 0) {
        info->episode = dashEp;
        for (int i = 1; i < n; i++)
            if (all_digits(toks[i]) && strlen(toks[i]) <= 3 && atoi(toks[i]) == dashEp) { epIdx = i; break; }
    }

    if (year == 0 && yearIdx > 0) year = atoi(toks[yearIdx]);
    info->year = year;

    int lim = n;
    if (epIdx >= 0) lim = epIdx;
    else if (yearIdx > 0) lim = yearIdx;
    if (firstHard > 0 && firstHard < lim) lim = firstHard;

    for (int i = 0; i < lim && info->ntitle < SF_MAX_TITLE; i++) {
        const char *t = toks[i];
        if (in_list(SOFT_NOISE, t) || in_list(STOPWORDS, t) || in_list(HARD_NOISE, t)) continue;
        strcpy(info->title[info->ntitle++], t);
    }
    for (int i = 0; i < n && info->nfull < SF_MAX_TOK; i++) {
        if (in_list(SOFT_NOISE, toks[i])) continue;
        strcpy(info->full[info->nfull++], toks[i]);
    }

    // Ten chi la mot so ("25.srt", "02.vi.srt") -> coi la so tap, tieu de lay tu thu muc
    if (info->episode < 0 && info->nfull == 1 && all_digits(info->full[0]) && strlen(info->full[0]) <= 3) {
        info->episode = atoi(info->full[0]);
        info->ntitle = 0;
    }
}

// ---------------------------------------------------------------- cham diem

static int tok_eq(const char *x, const char *y) {
    if (strcmp(x, y) == 0) return 1;
    size_t lx = strlen(x), ly = strlen(y);
    size_t m = lx < ly ? lx : ly;
    // chap nhan so it/nhieu ("dungeon"/"dungeons"): tien to >= 5 ky tu, lech toi da 1 ky tu
    if (m >= 5 && (lx > ly ? lx - ly : ly - lx) <= 1 && strncmp(x, y, m) == 0) return 1;
    return 0;
}

static int contains_tok(char arr[][SF_TOK_LEN], int n, const char *t) {
    for (int i = 0; i < n; i++)
        if (tok_eq(arr[i], t)) return 1;
    return 0;
}

// dem so token cua A co mat trong B
static int overlap(char A[][SF_TOK_LEN], int na, char B[][SF_TOK_LEN], int nb) {
    int c = 0;
    for (int i = 0; i < na; i++)
        if (contains_tok(B, nb, A[i])) c++;
    return c;
}

static double coeff(char A[][SF_TOK_LEN], int na, char B[][SF_TOK_LEN], int nb) {
    if (na == 0 || nb == 0) return 0.0;
    int o = overlap(A, na, B, nb);
    int m = na < nb ? na : nb;
    return (double)o / (double)m;
}

static void merge_union(char out[][SF_TOK_LEN], int *nout, char A[][SF_TOK_LEN], int na, char B[][SF_TOK_LEN], int nb, int cap) {
    *nout = 0;
    for (int i = 0; i < na && *nout < cap; i++) strcpy(out[(*nout)++], A[i]);
    for (int i = 0; i < nb && *nout < cap; i++)
        if (!contains_tok(out, *nout, B[i])) strcpy(out[(*nout)++], B[i]);
}

int sf_score(const char *videoName, const char *videoFolder, const char *subName, const char *subFolder) {
    SFInfo v, s, vf, sf;
    parse_info(videoName, &v);
    parse_info(subName, &s);
    parse_info(videoFolder, &vf);
    parse_info(subFolder, &sf);

    // 1) giong het sau khi bo duoi/ngon ngu -> 100
    if (v.nfull > 0 && v.nfull == s.nfull) {
        int same = 1;
        for (int i = 0; i < v.nfull; i++)
            if (strcmp(v.full[i], s.full[i]) != 0) { same = 0; break; }
        if (same) return 100;
    }

    // 2) tap/mua phai khop
    int vHasEp = v.episode >= 0, sHasEp = s.episode >= 0;
    if (vHasEp != sHasEp) return 0;
    if (vHasEp && v.episode != s.episode) return 0;
    if (v.season >= 0 && s.season >= 0 && v.season != s.season) return 0;

    // 3) do giong tieu de (ten file; neu thieu thi dung them ten thu muc)
    char Au[SF_MAX_TITLE * 2][SF_TOK_LEN], Bu[SF_MAX_TITLE * 2][SF_TOK_LEN];
    int nAu = 0, nBu = 0;
    merge_union(Au, &nAu, v.title, v.ntitle, vf.title, vf.ntitle, SF_MAX_TITLE * 2);
    merge_union(Bu, &nBu, s.title, s.ntitle, sf.title, sf.ntitle, SF_MAX_TITLE * 2);
    double sim1 = coeff(v.title, v.ntitle, s.title, s.ntitle);
    double sim2 = 0.85 * coeff(Au, nAu, Bu, nBu);
    double sim = sim1 > sim2 ? sim1 : sim2;

    if (vHasEp) {
        if (sim < 0.4) return 0;
        double sc = 40.0 + sim * 55.0;
        if (v.season >= 0 && s.season >= 0 && v.season == s.season) sc += 5.0;
        return sc > 99.0 ? 99 : (int)sc;
    }

    // phim le
    if (sim < 0.6) return 0;
    if (v.year && s.year && v.year != s.year) return 0;
    double sc = 30.0 + sim * 55.0;
    if (v.year && s.year && v.year == s.year) sc += 8.0;
    return sc > 99.0 ? 99 : (int)sc;
}

int sf_parse_se(const char *name, int *season, int *episode) {
    SFInfo i;
    parse_info(name, &i);
    if (season) *season = i.season;
    if (episode) *episode = i.episode;
    return i.episode >= 0;
}

void sf_describe(const char *name, char *out, unsigned long cap) {
    SFInfo i;
    parse_info(name, &i);
    int n = snprintf(out, cap, "S%d E%d Y%d [", i.season, i.episode, i.year);
    for (int k = 0; k < i.ntitle && n > 0 && (unsigned long)n + 40 < cap; k++) n += snprintf(out + n, cap - (unsigned long)n, "%s%s", k ? " " : "", i.title[k]);
    if (n > 0 && (unsigned long)n + 2 < cap) snprintf(out + n, cap - (unsigned long)n, "]");
}

const char *sf_language_from_name(const char *subName) {
    if (!subName) return NULL;
    char a[512], b[512], c[1024];
    snprintf(a, sizeof a, "%s", subName);
    strip_ext(a);
    int y = 0;
    strip_brackets(a, b, sizeof b, &y);
    // ngon ngu nam trong [] cung la tin hieu tot -> dung ca ban chua bo ngoac
    char both[1100];
    snprintf(both, sizeof both, "%s %s", a, b);
    normalize_phrase(both, c, sizeof c);

    char toks[SF_MAX_TOK][SF_TOK_LEN];
    int n = 0;
    for (char *p = c; *p && n < SF_MAX_TOK;) {
        while (*p == ' ') p++;
        if (!*p) break;
        int l = 0;
        while (*p && *p != ' ') { if (l < SF_TOK_LEN - 1) toks[n][l++] = *p; p++; }
        toks[n][l] = 0;
        n++;
    }
    for (int i = n - 1; i >= 0; i--)
        for (int k = 0; LANGS[k].tag; k++)
            if (strcmp(toks[i], LANGS[k].tag) == 0) return LANGS[k].lang;
    return NULL;
}

int sf_is_subtitle_ext(const char *ext) {
    if (!ext) return 0;
    char e[8];
    size_t l = strlen(ext);
    if (l == 0 || l > 5) return 0;
    for (size_t i = 0; i < l; i++) e[i] = (char)tolower((unsigned char)ext[i]);
    e[l] = 0;
    return strcmp(e, "srt") == 0 || strcmp(e, "ass") == 0 || strcmp(e, "ssa") == 0 || strcmp(e, "vtt") == 0;
}
