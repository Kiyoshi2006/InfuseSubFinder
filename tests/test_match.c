// Chay: gcc -Wall -Wextra -I../src ../src/SFMatch.c test_match.c -o test_match && ./test_match
#include <stdio.h>
#include <string.h>
#include "SFMatch.h"

static int fails = 0;
static int total = 0;

static void expect_ge(const char *label, int got, int min) {
    total++;
    if (got >= min) { printf("  ok   %-52s score=%d\n", label, got); }
    else { printf("  FAIL %-52s score=%d (can >= %d)\n", label, got, min); fails++; }
}
static void expect_eq(const char *label, int got, int want) {
    total++;
    if (got == want) { printf("  ok   %-52s score=%d\n", label, got); }
    else { printf("  FAIL %-52s score=%d (can == %d)\n", label, got, want); fails++; }
}
static void expect_lang(const char *name, const char *want) {
    const char *got = sf_language_from_name(name);
    total++;
    int ok = (want == NULL) ? (got == NULL) : (got && strcmp(got, want) == 0);
    printf("  %s   lang(%s) = %s\n", ok ? "ok  " : "FAIL", name, got ? got : "(null)");
    if (!ok) fails++;
}

int main(void) {
    sf_clear_aliases();
    const char *V = "Show.Name.S01E02.1080p.WEB.x265-GRP.mkv";

    puts("[phim bo]");
    expect_eq("giong het + .vi.srt", sf_score(V, "", "Show.Name.S01E02.1080p.WEB.x265-GRP.vi.srt", ""), 100);
    expect_ge("cung tap, khac release", sf_score(V, "", "Show Name S01E02 720p HDTV.srt", ""), 60);
    expect_ge("ten kieu 1x02", sf_score(V, "", "Show Name 1x02.vie.srt", ""), 60);
    expect_eq("sai tap (E03)", sf_score(V, "", "Show.Name.S01E03.vi.srt", ""), 0);
    expect_eq("sai mua (S02E02)", sf_score(V, "", "Show.Name.S02E02.vi.srt", ""), 0);
    expect_eq("khac phim (cung S01E02)", sf_score(V, "", "Another.Series.S01E02.vi.srt", ""), 0);
    expect_eq("sub phim le vs video tap", sf_score(V, "", "Show Name (2019).srt", ""), 0);

    puts("[anime / fansub]");
    const char *AV = "[SubsPlease] Dungeon ni Deai wo Motomeru no wa Machigatteiru Darou ka - 25 (1080p) [ABCD1234].mkv";
    expect_ge("cung ten, khac nhom", sf_score(AV, "", "[VNSub] Dungeon ni Deai wo Motomeru no wa Machigatteiru Darou ka - 25 [VN].srt", ""), 90);
    expect_eq("sai tap anime", sf_score(AV, "", "[VNSub] Dungeon ni Deai wo Motomeru no wa Machigatteiru Darou ka - 24.srt", ""), 0);

    puts("[ten viet tat khac nhau]");
    const char *DV = "Dungeon.ni.Deai.wo.Motomeru.no.wa.Machigatteiru.Darou.ka.S05E25.1080p.WEB.mkv";
    expect_eq("khong alias, khong thu muc -> khong khop", sf_score(DV, "", "Danmachi S05E25.vi.srt", ""), 0);
    expect_ge("khop nho thu muc video chua 'Danmachi'", sf_score(DV, "Danmachi", "Danmachi S05E25.vi.srt", ""), 60);
    expect_ge("khop nho thu muc sub chua ten", sf_score(DV, "", "25.srt", "Dungeon ni Deai wo Motomeru no wa Machigatteiru Darou ka VN Sub"), 60);
    sf_add_alias("danmachi", "dungeon ni deai wo motomeru no wa machigatteiru darou ka");
    expect_ge("khop nho alias", sf_score(DV, "", "Danmachi S05E25.vi.srt", ""), 60);
    sf_clear_aliases();

    puts("[phim le]");
    const char *M = "Inception.2010.1080p.BluRay.x264.mkv";
    expect_ge("cung phim + nam", sf_score(M, "", "Inception (2010) vie.srt", ""), 60);
    expect_ge("cung phim, khong nam", sf_score(M, "", "Inception.srt", ""), 60);
    expect_eq("sai nam", sf_score(M, "", "Inception.2011.srt", ""), 0);
    expect_eq("khac phim", sf_score(M, "", "Interstellar.2014.srt", ""), 0);
    expect_ge("phim co ten dai", sf_score("The.Lord.of.the.Rings.The.Fellowship.of.the.Ring.2001.1080p.mkv", "",
                                           "Lord of the Rings Fellowship of the Ring (2001).vi.srt", ""), 60);

    puts("[ngon ngu]");
    expect_lang("Show.S01E02.vi.srt", "Vietnamese");
    expect_lang("Show.S01E02.eng.srt", "English");
    expect_lang("[Group] Show - 02 [VietSub].srt", "Vietnamese");
    expect_lang("Show.S01E02.srt", NULL);

    puts("[duoi file]");
    expect_eq("srt", sf_is_subtitle_ext("SRT"), 1);
    expect_eq("ass", sf_is_subtitle_ext("ass"), 1);
    expect_eq("mkv", sf_is_subtitle_ext("mkv"), 0);

    printf("\n%d/%d dat\n", total - fails, total);
    return fails ? 1 : 0;
}
