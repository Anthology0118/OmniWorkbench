#pragma once

#include "ds_seed_common.hpp"

namespace omni {
namespace ds_seed {

inline void seed_ch4_ch6(std::vector<DsChapter>& chapters, std::vector<DsLesson>& lessons) {
    // =========================================================================
    // 第 4 章｜入门篇（2）——算法初步（《算法笔记》Ch4 全 7 节）
    // =========================================================================
    chapters.push_back({"ch4", 4, "算法与数学(Ch4-5)", "第 4 章｜算法初步全 7 节（排序/散列/递归/贪心/二分/双指针/递推）", "《算法笔记》Ch4 · 4.1-4.7 原书全节"});

    {
        DsLesson l; l.id = "4.1"; l.chapter_id = "ch4"; l.sort_order = 1;
        l.title = "4.1 排序：选择/插入排序与 sort 多级结构体排序";
        l.source_ref = "《算法笔记》4.1.1-4.1.3"; l.time_comp = "O(n log n)"; l.space_comp = "O(n)";
        l.viz_type = "two_ptr"; l.viz_label = "选择/插入基础排序 vs sort 结构体排名 cmp";
        l.tutorial_html = make_tut(
            "<strong>基础排序与工程竞赛实战排序：</strong>① 选择排序每次把后面最小元素放到前端；② 插入排序把当前牌插进左侧有序区间；③ 刷题一律直接写 C++ <code>sort(arr, arr + n, cmp)</code> 完成考场成绩与准考证号多关键字排名！",
            {
                {"cmp 函数写法（严禁 <=）", "<code>bool cmp(const Stu&amp; a, const Stu&amp; b) { return a.score != b.score ? a.score &gt; b.score : strcmp(a.id, b.id) &lt; 0; }</code>"},
                {"排定次序后算排名", "第一名 <code>r[0]=1</code>，后续 <code>if (stu[i].score == stu[i-1].score) r[i]=r[i-1]; else r[i]=i+1;</code>（常见 1,1,3 并列排名规则）。"}
            }, "",
            "对 <code>vector</code> / 数组使用自定义 <code>cmp</code> 时，当两元素相等必须返回 <code>false</code>！若错写成 <code>return a.x &gt;= b.x;</code> 会在大数据下引发 <code>sort</code> 段错误！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <string.h>
#include <algorithm>
using namespace std;

struct Stu { char id[16]; int score; };
bool cmp(const Stu& a, const Stu& b) {
    if (a.score != b.score) return a.score > b.score;
    return strcmp(a.id, b.id) < 0;
}

int main() {
    int n; scanf("%d", &n);
    Stu a[105];
    for (int i = 0; i < n; ++i) scanf("%15s %d", a[i].id, &a[i].score);
    sort(a, a + n, cmp);
    int rank = 1;
    for (int i = 0; i < n; ++i) {
        if (i > 0 && a[i].score != a[i - 1].score) rank = i + 1;
        printf("%s %d %d\n", a[i].id, rank, a[i].score);
    }
    return 0;
})CPP";
        add_ex(l, "ex_4_1", "练习：考生同分并列排名 (PAT A1025 简化)", "completion",
            "输入 n 名考生编号与分数，按分数降序（同分按编号字典序升序）排序，输出每名考生的“编号 排名 分数”（同分同排名，下一不同分跳过并列名次）。",
            "4\nB02 90\nA01 95\nA03 90\nC04 80", "A01 1 95\nA03 2 90\nB02 2 90\nC04 4 80",
            R"JSON([{"label":"并列排名输出","input":"4\nB02 90\nA01 95\nA03 90\nC04 80","expected":"A01 1 95\nA03 2 90\nB02 2 90\nC04 4 80"},{"label":"全满分","input":"2\n02 100\n01 100","expected":"01 1 100\n02 1 100"}])JSON",
            R"CPP(#include <stdio.h>
#include <string.h>
#include <algorithm>
using namespace std;
struct Stu { char id[16]; int score; };
int main() {
    int n; scanf("%d", &n);
    Stu s[105];
    for (int i = 0; i < n; ++i) scanf("%15s %d", s[i].id, &s[i].score);
    // TODO: 自定义 cmp 并用 sort 排序，再计算并列排名输出
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <string.h>
#include <algorithm>
using namespace std;
struct Stu { char id[16]; int score; };
bool cmp(const Stu& a, const Stu& b) {
    if (a.score != b.score) return a.score > b.score;
    return strcmp(a.id, b.id) < 0;
}
int main() {
    int n; scanf("%d", &n);
    Stu s[105];
    for (int i = 0; i < n; ++i) scanf("%15s %d", s[i].id, &s[i].score);
    sort(s, s + n, cmp);
    int r = 1;
    for (int i = 0; i < n; ++i) {
        if (i > 0 && s[i].score != s[i - 1].score) r = i + 1;
        printf("%s %d %d\n", s[i].id, r, s[i].score);
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "4.2"; l.chapter_id = "ch4"; l.sort_order = 2;
        l.title = "4.2 散列 (Hash)：整数直接定址与字符串进制 Hash";
        l.source_ref = "《算法笔记》4.2.1-4.2.2"; l.time_comp = "O(n + m)"; l.space_comp = "O(U)";
        l.viz_type = "bit"; l.viz_label = "数组下标直接充当计数桶 O(1) 检索出现频次";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》用「空间换时间」的思想精髓：散列（Hash 表）！</strong>把元素本身的值或字符串编码直接映射成数组下标，从而在 <code>O(1)</code> 内瞬间判断某元素是否出现过、出现了几次！",
            {
                {"整数散列（如 PAT 字符统计 / 旧键盘）", "把 ASCII 字符（0–127）或小整数直接当成数组下标 <code>hashTable[x]++</code>。"},
                {"字符串 Hash 初步（短串整数化）", "将仅含 26 个大写字母的 3 位编号串看作 26 进制数：<code>id = id * 26 + (s[i] - 'A')</code>（总状态数仅 <code>26^3 = 17576</code>）。"}
            }, "",
            "直接用数作为下标前提是值域可控（<code>≤ 10^6</code>）；若元素含负数，统一加上偏移量；若数值达到 <code>10^9</code> 需先离散化或直接用 STL <code>map</code> / <code>unordered_map</code>。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int hashTbl[100005];
int main() {
    int n, m, x;
    scanf("%d %d", &n, &m);
    for (int i = 0; i < n; ++i) { scanf("%d", &x); hashTbl[x]++; }
    for (int i = 0; i < m; ++i) {
        scanf("%d", &x);
        printf("%d\n", hashTbl[x]);
    }
    return 0;
})CPP";
        add_ex(l, "ex_4_2", "练习：旧键盘坏键检测 (字符散列 PAT B1029)", "completion",
            "输入两行字符串（原字符串 A 与实际输出字符串 B），不区分大小写（小写转大写）按 A 中首次出现顺序输出 A 中存在但 B 中缺失的字符（不重复输出）。",
            "7_This_is_a_test\n_hs_s_a_es", "7TI",
            R"JSON([{"label":"PAT B1029 旧键盘","input":"7_This_is_a_test\n_hs_s_a_es","expected":"7TI"},{"label":"无缺失","input":"abc\nABC","expected":""}])JSON",
            R"CPP(#include <stdio.h>
#include <string.h>
#include <ctype.h>
int main() {
    char a[105], b[105];
    scanf("%104s %104s", a, b);
    int inB[128] = {0}, printed[128] = {0};
    // TODO: 散列标记 b 中出现过的大写字符，再遍历 a 找出未出现在 b 且未输出过的字符
    printf("\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <string.h>
#include <ctype.h>
int main() {
    char a[105], b[105];
    scanf("%104s %104s", a, b);
    int inB[128] = {0}, printed[128] = {0};
    for (int i = 0; b[i]; ++i) inB[(unsigned char)toupper((unsigned char)b[i])] = 1;
    for (int i = 0; a[i]; ++i) {
        int ch = toupper((unsigned char)a[i]);
        if (!inB[ch] && !printed[ch]) {
            putchar(ch);
            printed[ch] = 1;
        }
    }
    putchar('\n');
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "4.3"; l.chapter_id = "ch4"; l.sort_order = 3;
        l.title = "4.3 分治与递归（全排列、N 皇后回溯与汉诺塔）";
        l.source_ref = "《算法笔记》4.3.1-4.3.2"; l.time_comp = "O(n!)"; l.space_comp = "O(n)";
        l.viz_type = "tree"; l.viz_label = "递归决策树生成排列与剪枝回溯";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》4.3 递归两大模型：全排列 (Full Permutation) 与 N 皇后：</strong>核心要点在于理解<strong>全局状态数组（如 <code>hashTable[x]</code>）如何与递归深度参数 <code>index</code> 配合完成现场保护与撤销回溯</strong>。",
            {
                {"1..n 全排列递归模板", "<code>void generateP(int idx)</code>：若 <code>idx == n + 1</code> 打印结果返回；否则枚举 <code>x = 1..n</code>，若 <code>!hashTable[x]</code> 则填入 <code>P[idx] = x</code>，置 <code>true</code> → 递归 <code>idx + 1</code> → 置回 <code>false</code>。"},
                {"汉诺塔递推步数", "将 <code>n</code> 个圆盘从 A 移到 C（借助 B）：先 <code>hanoi(n-1, A, C, B)</code>，搬底部大盘，再 <code>hanoi(n-1, B, A, C)</code>，总移动步数为 <code>2^n - 1</code>。"}
            }, "",
            "回溯写法中每次递归返回后<strong>必须恢复标记 <code>hashTable[x] = false</code></strong>，否则后续兄弟分支再也选不到这个数！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int n, P[15];
bool used[15];

void dfs(int idx) {
    if (idx == n + 1) {
        for (int i = 1; i <= n; ++i) printf("%s%d", i > 1 ? " " : "", P[i]);
        printf("\n");
        return;
    }
    for (int x = 1; x <= n; ++x) {
        if (!used[x]) {
            P[idx] = x; used[x] = true;
            dfs(idx + 1);
            used[x] = false;
        }
    }
}

int main() { scanf("%d", &n); dfs(1); return 0; })CPP";
        add_ex(l, "ex_4_3", "练习：递归回溯输出 1..n 全排列", "completion",
            "不使用 next_permutation，用递归回溯法按字典序输出 1 到 n (1 <= n <= 5) 的全部排列。",
            "3", "1 2 3\n1 3 2\n2 1 3\n2 3 1\n3 1 2\n3 2 1",
            R"JSON([{"label":"n=3 全排列","input":"3","expected":"1 2 3\n1 3 2\n2 1 3\n2 3 1\n3 1 2\n3 2 1"},{"label":"n=2","input":"2","expected":"1 2\n2 1"}])JSON",
            R"CPP(#include <stdio.h>
int n, P[10];
bool used[10];
void dfs(int idx) {
    // TODO: 实现全排列递归边界与回溯循环
}
int main() {
    scanf("%d", &n);
    dfs(1);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int n, P[10];
bool used[10];
void dfs(int idx) {
    if (idx == n + 1) {
        for (int i = 1; i <= n; ++i) printf("%s%d", i > 1 ? " " : "", P[i]);
        printf("\n");
        return;
    }
    for (int x = 1; x <= n; ++x) {
        if (!used[x]) {
            P[idx] = x; used[x] = true;
            dfs(idx + 1);
            used[x] = false;
        }
    }
}
int main() {
    scanf("%d", &n);
    dfs(1);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "4.4"; l.chapter_id = "ch4"; l.sort_order = 4;
        l.title = "4.4 贪心策略：简单贪心 (月饼) 与区间不相交贪心";
        l.source_ref = "《算法笔记》4.4.1-4.4.2"; l.time_comp = "O(n log n)"; l.space_comp = "O(n)";
        l.viz_type = "two_ptr"; l.viz_label = "优先选择单价最高 / 右端点最早的局部最优解";
        l.tutorial_html = make_tut(
            "<strong>贪心适用两大经典模型：</strong>① <strong>可拆分装包问题</strong>（如 PAT 月饼按单价从高到低装满需求）；② <strong>区间不相交问题</strong>（按区间右端点从小到大排序贪心选入，或《算法笔记》按左端降序选左端）。",
            {
                {"月饼小数精度与单价排序", "库存与总价在 PAT 里可能是正小数（用 <code>double</code> 存），按单价 <code>unit = total / store</code> 降序排列，够整份全卖，最后不够全吃下后立刻 <code>break</code>。"},
                {"区间选点 / 最多互不重叠区间", "按区间<strong>右端点升序</strong>，记录上一选取结束点 <code>lastR</code>，只要新区间的 <code>l >= lastR</code> 即选入。"}
            }, "",
            "与 01 背包区分：月饼/糖果允许切割部分重量时用<strong>贪心</strong>最简单；若每个物品不可分割只能整件选或不选，则<strong>不可用贪心</strong>，必须转第 11 章 01 背包动态规划！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;

struct Seg { int l, r; };
bool cmp(const Seg& a, const Seg& b) { return a.r != b.r ? a.r < b.r : a.l < b.l; }

int main() {
    int n; scanf("%d", &n);
    Seg s[1005];
    for (int i = 0; i < n; ++i) scanf("%d %d", &s[i].l, &s[i].r);
    sort(s, s + n, cmp);
    int cnt = 0, lastR = -2000000000;
    for (int i = 0; i < n; ++i) {
        if (s[i].l >= lastR) { cnt++; lastR = s[i].r; }
    }
    printf("%d\n", cnt);
    return 0;
})CPP";
        add_ex(l, "ex_4_4", "练习：最多不相交闭区间贪心", "completion",
            "输入 n 个区间 [l, r]（端点相接不冲突），求最多能选出的互不相交区间数目。",
            "4\n1 3\n2 4\n3 5\n6 7", "3",
            R"JSON([{"label":"闭区间相接","input":"4\n1 3\n2 4\n3 5\n6 7","expected":"3"},{"label":"大区间覆盖","input":"3\n1 10\n2 3\n4 5","expected":"2"}])JSON",
            R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;
struct Seg { int l, r; };
int main() {
    int n; scanf("%d", &n);
    Seg s[1005];
    for (int i = 0; i < n; ++i) scanf("%d %d", &s[i].l, &s[i].r);
    // TODO: 按右端点升序排好后贪心计数
    printf("0\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;
struct Seg { int l, r; };
bool cmp(const Seg& a, const Seg& b) { return a.r != b.r ? a.r < b.r : a.l < b.l; }
int main() {
    int n; scanf("%d", &n);
    Seg s[1005];
    for (int i = 0; i < n; ++i) scanf("%d %d", &s[i].l, &s[i].r);
    sort(s, s + n, cmp);
    int cnt = 0, lastR = -2000000000;
    for (int i = 0; i < n; ++i) {
        if (s[i].l >= lastR) { cnt++; lastR = s[i].r; }
    }
    printf("%d\n", cnt);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "4.5"; l.chapter_id = "ch4"; l.sort_order = 5;
        l.title = "4.5 二分查找、实数二分拓展与二进制快速幂";
        l.source_ref = "《算法笔记》4.5.1-4.5.3"; l.time_comp = "O(log n)"; l.space_comp = "O(1)";
        l.viz_type = "bsearch"; l.viz_label = "左闭右闭区间每次折半缩小一倍范围";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》4.5 三板斧：</strong>① 有序数组精确查找与手写 <code>lower_bound</code>（首个 <code>&gt;= x</code> 位置）；② <strong>二分答案与实数二分</strong>（装水问题/切木棒）；③ <strong>二分快速幂 <code>binaryPow(a, b, m)</code></strong>（将幂运算从 <code>O(b)</code> 压至 <code>O(log b)</code>）。",
            {
                {"最左边界（>= x）经典模板", "<code>while (l &lt; r) { int mid = l + (r - l) / 2; if (a[mid] &gt;= x) r = mid; else l = mid + 1; }</code>"},
                {"最右边界（二分答案求最大）", "注意向上取整防死循环：<code>mid = l + (r - l + 1) / 2;</code>"},
                {"迭代快速幂模板", "初始化 <code>ans = 1 % m;</code>，若 <code>b &amp; 1</code> 乘上当前底数并取模，每轮 <code>a = a * a % m; b &gt;&gt;= 1;</code>。"}
            }, "",
            "快速幂计算千万记着把 <code>a %= m</code> 和 <code>ans = 1 % m</code> 写上，防 <code>a &gt; m</code> 时第一次乘法溢出或 <code>m == 1</code> 特例出错。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

long long qpow(long long a, long long b, long long m) {
    long long ans = 1 % m;
    a %= m;
    while (b > 0) {
        if (b & 1) ans = ans * a % m;
        a = a * a % m;
        b >>= 1;
    }
    return ans;
}

int main() {
    long long a, b, m;
    scanf("%lld %lld %lld", &a, &b, &m);
    printf("%lld\n", qpow(a, b, m));
    return 0;
})CPP";
        add_ex(l, "ex_4_5", "练习：二进制迭代快速幂 (a^b mod m)", "completion",
            "输入非负整数 a, b 及正整数 m，计算 (a^b) % m（要求 O(log b) 且正确处理 m=1 与 10^9 级幂）。",
            "3 1000000000 1000000007", "235939645",
            R"JSON([{"label":"十亿次幂取模","input":"3 1000000000 1000000007","expected":"235939645"},{"label":"m=1特判","input":"7 0 1","expected":"0"},{"label":"2^10 mod 1000","input":"2 10 1000","expected":"24"}])JSON",
            R"CPP(#include <stdio.h>
long long qpow(long long a, long long b, long long m) {
    long long ans = 1 % m;
    // TODO: 实现二进制快速幂循环
    return ans;
}
int main() {
    long long a, b, m;
    scanf("%lld %lld %lld", &a, &b, &m);
    printf("%lld\n", qpow(a, b, m));
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
long long qpow(long long a, long long b, long long m) {
    long long ans = 1 % m;
    a %= m;
    while (b > 0) {
        if (b & 1) ans = ans * a % m;
        a = a * a % m;
        b >>= 1;
    }
    return ans;
}
int main() {
    long long a, b, m;
    scanf("%lld %lld %lld", &a, &b, &m);
    printf("%lld\n", qpow(a, b, m));
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "4.6"; l.chapter_id = "ch4"; l.sort_order = 6;
        l.title = "4.6 two pointers 双指针、归并排序与快速排序";
        l.source_ref = "《算法笔记》4.6.1-4.6.3"; l.time_comp = "O(n log n)"; l.space_comp = "O(n)";
        l.viz_type = "two_ptr"; l.viz_label = "双指针归并两个有序半区并顺带统计逆序对";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》把归并排序和快速排序专门放到 <code>two pointers</code> 这一节，直击本质：</strong>两个指针各自在有序区间或枢轴两侧单调行进，合计操作不超过 <code>O(n)</code>。",
            {
                {"序列合并与归并排序 (Merge Sort)", "分治对半分直到单元素，再用双指针 <code>i, j</code> 稳定合并；每次取右侧 <code>a[j]</code> 时左侧剩余 <code>mid - i + 1</code> 个均大于它（逆序对）。"},
                {"快速排序核心 Partition", "选基准值 <code>pivot = a[l]</code>，<code>j</code> 从右找小丢左边、<code>i</code> 从左找大丢右边直到 <code>i == j</code> 归位，递归左右。"}
            }, "",
            "手写快排切记加入随机化（先 <code>swap(a[l], a[l + rand() % (r - l + 1)])</code>）或三数取中，防止本身已排好序的数据让快排退化成 <code>O(n^2)</code>！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int a[100005], tmp[100005];
long long mergeSort(int l, int r) {
    if (l >= r) return 0;
    int mid = (l + r) / 2;
    long long inv = mergeSort(l, mid) + mergeSort(mid + 1, r);
    int i = l, j = mid + 1, k = l;
    while (i <= mid && j <= r) {
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else { inv += (mid - i + 1); tmp[k++] = a[j++]; }
    }
    while (i <= mid) tmp[k++] = a[i++];
    while (j <= r) tmp[k++] = a[j++];
    for (int p = l; p <= r; ++p) a[p] = tmp[p];
    return inv;
}

int main() {
    int n; scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    printf("%lld\n", mergeSort(0, n - 1));
    return 0;
})CPP";
        add_ex(l, "ex_4_6", "练习：归并排序统计逆序对数量", "completion",
            "输入 n 个整数，使用归并排序在 O(n log n) 时间内统计全部逆序对总数（i < j 且 a[i] > a[j]）。",
            "5\n5 4 3 2 1", "10",
            R"JSON([{"label":"完全逆序","input":"5\n5 4 3 2 1","expected":"10"},{"label":"普通乱序","input":"4\n2 4 1 3","expected":"3"}])JSON",
            R"CPP(#include <stdio.h>
int a[100005], tmp[100005];
long long mergeSort(int l, int r) {
    if (l >= r) return 0;
    int mid = (l + r) / 2;
    long long inv = mergeSort(l, mid) + mergeSort(mid + 1, r);
    // TODO: 合并 [l..mid] 与 [mid+1..r] 并累加 inv
    return inv;
}
int main() {
    int n; scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    printf("%lld\n", mergeSort(0, n - 1));
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int a[100005], tmp[100005];
long long mergeSort(int l, int r) {
    if (l >= r) return 0;
    int mid = (l + r) / 2;
    long long inv = mergeSort(l, mid) + mergeSort(mid + 1, r);
    int i = l, j = mid + 1, k = l;
    while (i <= mid && j <= r) {
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else { inv += (mid - i + 1); tmp[k++] = a[j++]; }
    }
    while (i <= mid) tmp[k++] = a[i++];
    while (j <= r) tmp[k++] = a[j++];
    for (int p = l; p <= r; ++p) a[p] = tmp[p];
    return inv;
}
int main() {
    int n; scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    printf("%lld\n", mergeSort(0, n - 1));
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "4.7"; l.chapter_id = "ch4"; l.sort_order = 7;
        l.title = "4.7 打表、活用递推 (几个 PAT) 与随机选择 randSelect";
        l.source_ref = "《算法笔记》4.7.1-4.7.3"; l.time_comp = "O(n)"; l.space_comp = "O(n)";
        l.viz_type = "dp"; l.viz_label = "PAT 计数：对每个 'A'，左边 P 数 × 右边 T 数累加！";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》非常惊艳的一节（4.7 活用递推 + 随机选择）：</strong>① 问字符串里有多少个 <code>\"PAT\"</code> 子序列？不用三重循环，只统计每个 <code>'A'</code> 左边 <code>'P'</code> 个数 × 右边 <code>'T'</code> 个数；② 用快排 <code>Partition</code> + 随机数能在期望 <code>O(n)</code> 内求第 <code>K</code> 大或均分集合差！",
            {
                {"\"几个 PAT\" 两遍扫描 O(n)", "正序记录 <code>leftNumP[i]</code>；倒序累加 <code>rightNumT</code>，遇到 <code>s[i] == 'A'</code> 就 <code>ans = (ans + leftNumP[i] * rightNumT) % 1000000007;</code>！"},
                {"随机选择 randSelect(A, l, r, K)", "把区间随机 Partition 后基准排第 <code>M</code> 名：若 <code>K == M</code> 直接返回；<code>K &lt; M</code> 仅递归左边，<code>K &gt; M</code> 递归右边第 <code>K - M</code> 小，省掉一半分支做到期望线性！"}
            }, "",
            "\"几个 PAT\" 取模常数是 <code>1000000007</code>，每次加完务必取模且 <code>ans</code> 和乘积建议用 <code>long long</code>！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <string.h>

const long long MOD = 1000000007LL;
char s[100005];
long long leftP[100005];

int main() {
    scanf("%100000s", s);
    int n = (int)strlen(s);
    for (int i = 0; i < n; ++i) {
        if (i > 0) leftP[i] = leftP[i - 1];
        if (s[i] == 'P') leftP[i]++;
    }
    long long ans = 0, rightT = 0;
    for (int i = n - 1; i >= 0; --i) {
        if (s[i] == 'T') rightT++;
        else if (s[i] == 'A') ans = (ans + leftP[i] * rightT) % MOD;
    }
    printf("%lld\n", ans);
    return 0;
})CPP";
        add_ex(l, "ex_4_7", "练习：O(n) 统计子序列 \"PAT\" 个数 (PAT B1040)", "completion",
            "输入仅含 P、A、T 字符的字符串，输出其中可组成 \"PAT\" 子序列的总数 mod 1000000007。",
            "APPAPT", "2",
            R"JSON([{"label":"PAT B1040 样例","input":"APPAPT","expected":"2"},{"label":"PPAT","input":"PPAT","expected":"2"},{"label":"无 A","input":"PPT","expected":"0"}])JSON",
            R"CPP(#include <stdio.h>
#include <string.h>
const long long MOD = 1000000007LL;
char s[100005];
long long leftP[100005];
int main() {
    scanf("%100000s", s);
    int n = (int)strlen(s);
    // TODO: 统计每个 'A' 左侧 P 数量 * 右侧 T 数量之和 mod MOD
    printf("0\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <string.h>
const long long MOD = 1000000007LL;
char s[100005];
long long leftP[100005];
int main() {
    scanf("%100000s", s);
    int n = (int)strlen(s);
    for (int i = 0; i < n; ++i) {
        if (i > 0) leftP[i] = leftP[i - 1];
        if (s[i] == 'P') leftP[i]++;
    }
    long long ans = 0, rightT = 0;
    for (int i = n - 1; i >= 0; --i) {
        if (s[i] == 'T') rightT++;
        else if (s[i] == 'A') ans = (ans + leftP[i] * rightT) % MOD;
    }
    printf("%lld\n", ans);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 5 章｜入门篇（3）——数学问题（《算法笔记》Ch5 全 8 节）
    // =========================================================================
    chapters.push_back({"ch5", 5, "算法与数学(Ch4-5)", "第 5 章｜数学问题全 8 节（黑洞/GCD/分数/素数筛/大整数/exgcd/组合）", "《算法笔记》Ch5 · 5.1-5.8 原书全节"});

    {
        DsLesson l; l.id = "5.1"; l.chapter_id = "ch5"; l.sort_order = 1;
        l.title = "5.1-5.2 简单数学、数字黑洞 6174 与 GCD/LCM";
        l.source_ref = "《算法笔记》5.1-5.2"; l.time_comp = "O(log a)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "欧几里得辗转相除法 gcd(a, b) = gcd(b, a % b)";
        l.tutorial_html = make_tut(
            "<strong>简单数学规律与欧几里得最大公约数：</strong>任意 4 位不全相同整数，把各位数字重排为最大数减去最小数，不出 7 步必掉入黑洞 <strong><code>6174</code></strong>！而 <code>gcd(a, b) = b == 0 ? a : gcd(b, a % b)</code> 是分数化简与扩展欧几里得的地基。",
            {
                {"数字黑洞循环写法", "注意数字转 4 数组 <code>to_array</code> 即使不足 4 位也要前面补 <code>0</code>（例如 <code>100</code> 看成 <code>0100</code>，重排最大 <code>1000</code> 减最小 <code>0001 = 0999</code>）！"},
                {"最小公倍数防溢出", "必须写成 <code>a / gcd(a, b) * b</code>（先除后乘）。"}
            }, "",
            "若题给四位数字四个数全相等（如 <code>2222</code>），第一步相减得 <code>0000</code> 就退出循环！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

long long gcd(long long a, long long b) { return b == 0 ? a : gcd(b, a % b); }

int main() {
    long long a, b; scanf("%lld %lld", &a, &b);
    long long g = gcd(a, b);
    printf("gcd=%lld lcm=%lld\n", g, a / g * b);
    return 0;
})CPP";
        add_ex(l, "ex_5_1", "练习：数字黑洞 6174 迭代步数 (PAT B1019)", "completion",
            "输入正整数 N (1..9999，按四位数前面补0)。若四位完全相同输出 0；否则反复求「最大重排数 - 最小重排数」直到等于 6174，输出经过的步骤数。",
            "6767", "4",
            R"JSON([{"label":"6767->6174 4步","input":"6767","expected":"4"},{"label":"就是6174","input":"6174","expected":"1"},{"label":"四位全等","input":"2222","expected":"0"}])JSON",
            R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;
int main() {
    int n, steps = 0; scanf("%d", &n);
    // TODO: 拆四位数组排序拼最大/最小，注意 6174 也走至少一轮、若相减=0输出0并结束
    printf("%d\n", steps);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;
int main() {
    int n, steps = 0; scanf("%d", &n);
    while (true) {
        int d[4];
        for (int i = 0; i < 4; ++i) { d[i] = n % 10; n /= 10; }
        sort(d, d + 4);
        int mn = d[0]*1000 + d[1]*100 + d[2]*10 + d[3];
        int mx = d[3]*1000 + d[2]*100 + d[1]*10 + d[0];
        if (mx == mn) { steps = 0; break; }
        n = mx - mn;
        steps++;
        if (n == 6174) break;
    }
    printf("%d\n", steps);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "5.3"; l.chapter_id = "ch5"; l.sort_order = 2;
        l.title = "5.3 分数的表示、约分化简与四则运算";
        l.source_ref = "《算法笔记》5.3.1-5.3.3"; l.time_comp = "O(log V)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "Fraction{up, down} 保持分母为正、分子带符号且互质";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》Fraction 分数化简黄金三条规范：</strong>① 使分母 <code>down</code> 永远非负，若为负则分子分母同时取反；② 如果分子 <code>up == 0</code>，强制把分母置为 <code>1</code>；③ 分子分母同时除以 <code>gcd(abs(up), abs(down))</code> 完成最简约分。",
            {
                {"Fraction 化简函数 reduction", "每次做完加、减、乘、除之后立刻调用一次 <code>reduction(res)</code>，避免后续步骤分母累乘溢出 <code>long long</code>。"},
                {"带分数输出格式规范", "如果 <code>r.down == 1</code> 直接输出整数 <code>%lld</code>；如果是假分数（<code>abs(r.up) &gt; r.down</code>），输出 <code>r.up / r.down</code> 整数部分加 <code>abs(r.up) % r.down / r.down</code>。"}
            }, "",
            "如果测试数据包含分数相加结果在 10^9 级分母，<code>up</code> 和 <code>down</code> 一定要开 <code>long long</code>！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <stdlib.h>

struct Fraction { long long up, down; };
long long gcd(long long a, long long b) { return b == 0 ? a : gcd(b, a % b); }
Fraction reduction(Fraction r) {
    if (r.down < 0) { r.up = -r.up; r.down = -r.down; }
    if (r.up == 0) r.down = 1;
    else {
        long long d = gcd(r.up < 0 ? -r.up : r.up, r.down);
        r.up /= d; r.down /= d;
    }
    return r;
}

int main() {
    Fraction a{1, 3}, b{1, 6};
    Fraction c{a.up * b.down + b.up * a.down, a.down * b.down};
    c = reduction(c);
    printf("%lld/%lld\n", c.up, c.down);
    return 0;
})CPP";
        add_ex(l, "ex_5_3", "练习：N 个分数相加并输出最简真分数/假分数", "completion",
            "输入 n 个形如 a/b 的分数，累加总和并以最简形式输出（若分母为1只输出分子，若带整数部分如 7/3 输出 \"2 1/3\"）。",
            "2\n1/2 1/6", "2/3",
            R"JSON([{"label":"真分数","input":"2\n1/2 1/6","expected":"2/3"},{"label":"整数结果","input":"2\n1/2 1/2","expected":"1"},{"label":"带分数","input":"2\n5/3 2/3","expected":"2 1/3"}])JSON",
            R"CPP(#include <stdio.h>
struct Frac { long long up, down; };
long long gcd(long long a, long long b) { return b == 0 ? a : gcd(b, a % b); }
Frac norm(Frac f) {
    // TODO: 归一化分母为正、0化为 0/1、除最大公约数
    return f;
}
int main() {
    int n; scanf("%d", &n);
    Frac sum{0, 1}, cur;
    for (int i = 0; i < n; ++i) {
        scanf("%lld/%lld", &cur.up, &cur.down);
        sum = norm({sum.up * cur.down + cur.up * sum.down, sum.down * cur.down});
    }
    if (sum.down == 1) printf("%lld\n", sum.up);
    else if ((sum.up < 0 ? -sum.up : sum.up) > sum.down)
        printf("%lld %lld/%lld\n", sum.up / sum.down, (sum.up < 0 ? -sum.up : sum.up) % sum.down, sum.down);
    else printf("%lld/%lld\n", sum.up, sum.down);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
struct Frac { long long up, down; };
long long gcd(long long a, long long b) { return b == 0 ? a : gcd(b, a % b); }
Frac norm(Frac f) {
    if (f.down < 0) { f.up = -f.up; f.down = -f.down; }
    if (f.up == 0) f.down = 1;
    else {
        long long g = gcd(f.up < 0 ? -f.up : f.up, f.down);
        f.up /= g; f.down /= g;
    }
    return f;
}
int main() {
    int n; scanf("%d", &n);
    Frac sum{0, 1}, cur;
    for (int i = 0; i < n; ++i) {
        scanf("%lld/%lld", &cur.up, &cur.down);
        sum = norm({sum.up * cur.down + cur.up * sum.down, sum.down * cur.down});
    }
    if (sum.down == 1) printf("%lld\n", sum.up);
    else if ((sum.up < 0 ? -sum.up : sum.up) > sum.down)
        printf("%lld %lld/%lld\n", sum.up / sum.down, (sum.up < 0 ? -sum.up : sum.up) % sum.down, sum.down);
    else printf("%lld/%lld\n", sum.up, sum.down);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "5.4"; l.chapter_id = "ch5"; l.sort_order = 3;
        l.title = "5.4-5.5 素数判定、筛法素数表与质因子分解";
        l.source_ref = "《算法笔记》5.4-5.5"; l.time_comp = "O(sqrt(n)) / O(n)"; l.space_comp = "O(n)";
        l.viz_type = "complexity"; l.viz_label = "埃氏筛 / 欧拉线性筛 & 根号试除分解质因数";
        l.tutorial_html = make_tut(
            "<strong>素数检测与分解完整策略：</strong>① 单个大数判素只需枚举 <code>i = 2 .. sqrt(n)</code>；② 批量筛 `[2..N]` 用<strong>埃氏筛</strong>（<code>O(n log log n)</code>）或<strong>欧拉线性筛</strong>（<code>O(n)</code>）；③ 分解质因数只枚举到 <code>sqrt(n)</code>，结束后若剩余 <code>n &gt; 1</code>，说明它本身就是最后那个大质因子！",
            {
                {"质因子结构体分解（5.5 节）", "预先打素数表后试除每个素数 <code>p[i] &lt;= sqrt(n)</code>，统计幂次 <code>fac[num].cnt++</code>。"},
                {"边界特判 0 和 1", "<code>isPrime(n)</code> 开头必写 <code>if (n &lt;= 1) return false;</code>！"}
            }, "",
            "循环写 <code>i * i &lt;= n</code> 遇到 <code>n</code> 靠近 <code>2^31 - 1</code> 时会溢出变成死循环！改成 <code>i &lt;= n / i</code> 或先算 <code>int sqr = sqrt(1.0 * n);</code>。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    long long n; scanf("%lld", &n);
    int first = 1;
    for (long long i = 2; i * i <= n; ++i) {
        if (n % i == 0) {
            int cnt = 0;
            while (n % i == 0) { n /= i; cnt++; }
            if (!first) printf("*");
            printf("%lld^%d", i, cnt);
            first = 0;
        }
    }
    if (n > 1) { if (!first) printf("*"); printf("%lld^1", n); }
    printf("\n");
    return 0;
})CPP";
        add_ex(l, "ex_5_4", "练习：质因子分解标准输出 (PAT A1059 简化)", "completion",
            "输入大于 1 的正整数 n，按质因子升序输出其分解式，格式为 p^k 相乘（如 12 -> 2^2*3^1）。",
            "180", "2^2*3^2*5^1",
            R"JSON([{"label":"合数分解 180","input":"180","expected":"2^2*3^2*5^1"},{"label":"素数 97","input":"97","expected":"97^1"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    long long n; scanf("%lld", &n);
    // TODO: 试除 2..sqrt(n) 输出质因子幂次，最后别忘了若 n > 1 输出 n^1
    printf("\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    long long n; scanf("%lld", &n);
    int first = 1;
    for (long long i = 2; i <= n / i; ++i) {
        if (n % i == 0) {
            int cnt = 0;
            while (n % i == 0) { n /= i; cnt++; }
            if (!first) printf("*");
            printf("%lld^%d", i, cnt);
            first = 0;
        }
    }
    if (n > 1) { if (!first) printf("*"); printf("%lld^1", n); }
    printf("\n");
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "5.6"; l.chapter_id = "ch5"; l.sort_order = 4;
        l.title = "5.6 大整数存储 (bign) 与高精度加减乘除";
        l.source_ref = "《算法笔记》5.6.1-5.6.2"; l.time_comp = "O(L)"; l.space_comp = "O(L)";
        l.viz_type = "two_ptr"; l.viz_label = "低位下标 0 在前方便进位扩容，打印时倒序输出";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》原书 <code>struct bign { int d[1000], len; }</code> 大整数黄金规则：</strong>整数低位存放在 <code>d[0]</code>，高位放在 <code>d[len - 1]</code>，严格对齐小学竖式加减乘除逻辑。",
            {
                {"高精度加法/减法核心", "加法逐位 <code>temp = a.d[i] + b.d[i] + carry</code>；减法遇到不够减直接向高位借 1：<code>a.d[i+1]--; a.d[i] += 10;</code>，末尾记得去掉高位的 <code>0</code>（保留至少 1 位）。"},
                {"高精度乘/除低精度整数 b", "乘法：<code>temp = a.d[i] * b + carry; c.d[len++] = temp % 10; carry = temp / 10;</code>；除法从高位向低位逐位带余数 <code>r = r * 10 + a.d[i]</code>。"}
            }, "",
            "减法前若被减数 <code>a &lt; b</code>，必须先比较两数大小（先比位数，再从高位向下逐位比），输出负号后反过来算 <code>b - a</code>！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <string.h>

struct bign { int d[1005], len; bign() { memset(d, 0, sizeof(d)); len = 0; } };
bign parseBign(const char* s) {
    bign a; a.len = (int)strlen(s);
    for (int i = 0; i < a.len; ++i) a.d[i] = s[a.len - 1 - i] - '0';
    return a;
}
bign addBign(bign a, bign b) {
    bign c; int carry = 0;
    for (int i = 0; i < a.len || i < b.len; ++i) {
        int t = a.d[i] + b.d[i] + carry;
        c.d[c.len++] = t % 10; carry = t / 10;
    }
    if (carry) c.d[c.len++] = carry;
    return c;
}

int main() {
    char sa[1005], sb[1005]; scanf("%1000s %1000s", sa, sb);
    bign c = addBign(parseBign(sa), parseBign(sb));
    for (int i = c.len - 1; i >= 0; --i) printf("%d", c.d[i]);
    printf("\n");
    return 0;
})CPP";
        add_ex(l, "ex_5_6", "练习：bign 高精度加法竖式实现", "completion",
            "按《算法笔记》struct bign 逆序低位在前存储规则，补全 addBign 实现两个超长非负大整数相加。",
            "99999999999999999999 1", "100000000000000000000",
            R"JSON([{"label":"连续进位","input":"99999999999999999999 1","expected":"100000000000000000000"},{"label":"不等长加法","input":"123456789012345 987654","expected":"123456789999999"}])JSON",
            R"CPP(#include <stdio.h>
#include <string.h>
struct bign { int d[1005], len; bign() { memset(d, 0, sizeof(d)); len = 0; } };
bign fromStr(const char* s) {
    bign a; a.len = (int)strlen(s);
    for (int i = 0; i < a.len; ++i) a.d[i] = s[a.len - 1 - i] - '0';
    return a;
}
bign addBign(bign a, bign b) {
    bign c;
    // TODO: 逐位相加进位生成 c
    return c;
}
int main() {
    char s1[1005], s2[1005]; scanf("%1000s %1000s", s1, s2);
    bign c = addBign(fromStr(s1), fromStr(s2));
    for (int i = c.len - 1; i >= 0; --i) printf("%d", c.d[i]);
    printf("\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <string.h>
struct bign { int d[1005], len; bign() { memset(d, 0, sizeof(d)); len = 0; } };
bign fromStr(const char* s) {
    bign a; a.len = (int)strlen(s);
    for (int i = 0; i < a.len; ++i) a.d[i] = s[a.len - 1 - i] - '0';
    return a;
}
bign addBign(bign a, bign b) {
    bign c; int carry = 0;
    for (int i = 0; i < a.len || i < b.len; ++i) {
        int t = a.d[i] + b.d[i] + carry;
        c.d[c.len++] = t % 10;
        carry = t / 10;
    }
    if (carry > 0) c.d[c.len++] = carry;
    return c;
}
int main() {
    char s1[1005], s2[1005]; scanf("%1000s %1000s", s1, s2);
    bign c = addBign(fromStr(s1), fromStr(s2));
    for (int i = c.len - 1; i >= 0; --i) printf("%d", c.d[i]);
    printf("\n");
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "5.7"; l.chapter_id = "ch5"; l.sort_order = 5;
        l.title = "5.7-5.8 扩展欧几里得 exGcd 与 n! 质因子 / 组合数计算";
        l.source_ref = "《算法笔记》5.7-5.8"; l.time_comp = "O(log a)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "扩展欧几里得同步求解 ax + by = gcd(a, b) 并求逆元";
        l.tutorial_html = make_tut(
            "<strong>数论进阶终极两板子：</strong>① <strong>扩展欧几里得 <code>exGcd(a, b, x, y)</code></strong> 求 <code>ax + by = 1</code>，其中 <code>(x % m + m) % m</code> 就是任意互质模数下的乘法逆元；② 算 <code>n!</code> 末尾有多少个 <code>0</code>（或质因子 <code>p</code> 的个数）：<code>ans += n / p; n /= p;</code> 仅需 3 行代码！",
            {
                {"exGcd C++ 引用递归 4 行", "<code>if (b == 0) { x = 1; y = 0; return a; } long long g = exGcd(b, a % b, x, y); long long tmp = x; x = y; y = tmp - (a / b) * y; return g;</code>"},
                {"组合数 C(n, m) 边乘边除写法", "不用求阶乘再除，写 <code>for (long long i = 1; i <= m; ++i) ans = ans * (n - m + i) / i;</code>（中间必整除，<code>n ≤ 62</code> 不溢出）。"}
            }, "",
            "求 <code>(a * x) % m == 1</code> 最小正整数解时，因 <code>exGcd</code> 算出的 <code>x</code> 可能为负，务必写成 <code>(x % m + m) % m</code> 转正！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

long long exGcd(long long a, long long b, long long& x, long long& y) {
    if (b == 0) { x = 1; y = 0; return a; }
    long long g = exGcd(b, a % b, x, y);
    long long tmp = x; x = y; y = tmp - (a / b) * y;
    return g;
}

int main() {
    long long a, m, x, y; scanf("%lld %lld", &a, &m);
    if (exGcd(a, m, x, y) != 1) printf("-1\n");
    else printf("%lld\n", (x % m + m) % m);
    return 0;
})CPP";
        add_ex(l, "ex_5_7", "练习：扩展欧几里得求乘法逆元", "completion",
            "输入正整数 a, m，求同余方程 a * x ≡ 1 (mod m) 的最小正整数解；若无解输出 -1。",
            "3 11", "4",
            R"JSON([{"label":"3^-1 mod 11 = 4","input":"3 11","expected":"4"},{"label":"不互质无解","input":"6 9","expected":"-1"}])JSON",
            R"CPP(#include <stdio.h>
long long exGcd(long long a, long long b, long long& x, long long& y) {
    // TODO: 递归求解 ax + by = gcd(a, b)
    return 1;
}
int main() {
    long long a, m, x = 0, y = 0;
    scanf("%lld %lld", &a, &m);
    if (exGcd(a, m, x, y) != 1) printf("-1\n");
    else printf("%lld\n", (x % m + m) % m);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
long long exGcd(long long a, long long b, long long& x, long long& y) {
    if (b == 0) { x = 1; y = 0; return a; }
    long long g = exGcd(b, a % b, x, y);
    long long tmp = x; x = y; y = tmp - (a / b) * y;
    return g;
}
int main() {
    long long a, m, x = 0, y = 0;
    scanf("%lld %lld", &a, &m);
    if (exGcd(a, m, x, y) != 1) printf("-1\n");
    else printf("%lld\n", (x % m + m) % m);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 6 章｜C++ 标准模板库 (STL)（C 正式进阶 C++，全 9 节）
    // =========================================================================
    chapters.push_back({"ch6", 6, "C++ STL(Ch6)", "第 6 章｜从 C 跃升 C++：STL 容器与 <algorithm>（全 9 节）", "《算法笔记》Ch6 · 6.1-6.9 · 前导 PPT 指定必修"});

    {
        DsLesson l; l.id = "6.1"; l.chapter_id = "ch6"; l.sort_order = 1;
        l.title = "6.1 vector 变长数组常用操作详解";
        l.source_ref = "《算法笔记》6.1"; l.time_comp = "O(1) 尾插"; l.space_comp = "O(n)";
        l.viz_type = "two_ptr"; l.viz_label = "vector 自动倍增内存，比定长 C 数组更灵活";
        l.tutorial_html = make_tut(
            "<strong>正式进入 C++ STL 领域！</strong>用 <code>vector&lt;int&gt;</code> 取代死板的定长 C 数组，既支持下标 <code>v[i]</code> 与动态建图（邻接表 <code>vector&lt;int&gt; adj[N]</code>），又内置 <code>size()</code>、<code>clear()</code>、<code>pop_back()</code>、<code>insert()</code> 和 <code>erase()</code>。",
            {
                {"三种常用初始化", "<code>vector&lt;int&gt; v;</code>（空）、<code>vector&lt;int&gt; v(n, 0);</code>（长 n 全 0）、<code>vector&lt;vector&lt;int&gt;&gt; g(n);</code>（二维变长矩阵）。"},
                {"erase 删除元素区别", "<code>v.erase(it)</code> 删单个迭代器；<code>v.erase(first, last)</code> 删左闭右开区间（去重必背 <code>v.erase(unique(v.begin(), v.end()), v.end());</code>）。"}
            }, "",
            "向空 <code>vector</code> 里不能直接写 <code>v[0] = 5</code>（未分配下标直接越界崩！），应先指定大小 <code>vector&lt;int&gt; v(n);</code> 或用 <code>v.push_back(5);</code>。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    vector<int> v = {4, 1, 2, 4, 3, 1};
    sort(v.begin(), v.end());
    v.erase(unique(v.begin(), v.end()), v.end());
    for (int x : v) cout << x << " ";
    return 0;
})CPP";
        add_ex(l, "ex_6_1", "练习：vector 动态读入并排序去重", "completion",
            "输入 n 个整数存入 vector，用 sort + unique + erase 排序并去掉重复数，按升序输出。",
            "6\n4 2 2 9 4 1", "1 2 4 9",
            R"JSON([{"label":"排序去重","input":"6\n4 2 2 9 4 1","expected":"1 2 4 9"},{"label":"全部相等","input":"3\n5 5 5","expected":"5"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> v(n);
    for (int i = 0; i < n; ++i) cin >> v[i];
    // TODO: sort + unique + erase 去重
    for (size_t i = 0; i < v.size(); ++i) cout << (i ? " " : "") << v[i];
    cout << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n; cin >> n;
    vector<int> v(n);
    for (int i = 0; i < n; ++i) cin >> v[i];
    sort(v.begin(), v.end());
    v.erase(unique(v.begin(), v.end()), v.end());
    for (size_t i = 0; i < v.size(); ++i) cout << (i ? " " : "") << v[i];
    cout << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "6.2"; l.chapter_id = "ch6"; l.sort_order = 2;
        l.title = "6.2 set / multiset 自动排序去重集合";
        l.source_ref = "《算法笔记》6.2"; l.time_comp = "O(log n)"; l.space_comp = "O(n)";
        l.viz_type = "bst"; l.viz_label = "红黑树保持元素严格升序、支持 O(log n) 找与删";
        l.tutorial_html = make_tut(
            "<strong>无需手动实现去重和排顺序：<code>set&lt;int&gt;</code> 插入的同时就在内部红黑树里完成了自动去重与严格升序排列！</strong>",
            {
                {"set 高频成员函数", "<code>s.insert(x)</code>、<code>s.find(x)</code>（找不到返回 <code>s.end()</code>）、<code>s.erase(x)</code>、<code>s.count(x)</code>（判存在）。"},
                {"set 遍历与 multiset", "<code>set</code> 不支持下标 <code>s[0]</code>，必须用迭代器 <code>for (auto x : s)</code> 遍历；若需允许重复元素请用 <code>multiset</code>。"}
            }, "",
            "在 <code>multiset</code> 中若只想删掉 1 个值为 <code>x</code> 的副本，一定要写 <code>ms.erase(ms.find(x))</code>（先判 <code>!= end()</code>），直接 <code>ms.erase(x)</code> 会清空全部相同值！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <set>
using namespace std;

int main() {
    int n, x; cin >> n;
    set<int> s;
    for (int i = 0; i < n; ++i) { cin >> x; s.insert(x); }
    for (int val : s) cout << val << " ";
    return 0;
})CPP";
        add_ex(l, "ex_6_2", "练习：用 set 计算集合并集并按升序输出", "completion",
            "先输入集合 A 的元素数 n 及 n 个数，再输入集合 B 的元素数 m 及 m 个数，用 set 输出 A∪B 的升序元素序列。",
            "3\n1 3 5\n3\n2 3 4", "1 2 3 4 5",
            R"JSON([{"label":"集合并集","input":"3\n1 3 5\n3\n2 3 4","expected":"1 2 3 4 5"},{"label":"全重叠","input":"2\n7 8\n2\n8 7","expected":"7 8"}])JSON",
            R"CPP(#include <iostream>
#include <set>
using namespace std;
int main() {
    set<int> s;
    int n, m, x;
    cin >> n; for (int i = 0; i < n; ++i) { cin >> x; /* TODO */ }
    cin >> m; for (int i = 0; i < m; ++i) { cin >> x; /* TODO */ }
    int first = 1;
    for (int v : s) { cout << (first ? "" : " ") << v; first = 0; }
    cout << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <set>
using namespace std;
int main() {
    set<int> s;
    int n, m, x;
    cin >> n; for (int i = 0; i < n; ++i) { cin >> x; s.insert(x); }
    cin >> m; for (int i = 0; i < m; ++i) { cin >> x; s.insert(x); }
    int first = 1;
    for (int v : s) { cout << (first ? "" : " ") << v; first = 0; }
    cout << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "6.3"; l.chapter_id = "ch6"; l.sort_order = 3;
        l.title = "6.3 string 彻底告别 char[] 的边界噩梦";
        l.source_ref = "《算法笔记》6.3"; l.time_comp = "O(L)"; l.space_comp = "O(L)";
        l.viz_type = "two_ptr"; l.viz_label = "string 支持 + 拼接、== 比值、substr 切片与 find 检索";
        l.tutorial_html = make_tut(
            "<strong>告别 C 字符数组的繁琐 <code>strcpy / strcat / strcmp</code>：</strong>C++ <code>string</code> 支持直接写 <code>if (a == b)</code> 判等、<code>if (a &lt; b)</code> 比字典序、<code>s1 + s2</code> 直接拼接！",
            {
                {"常用四大高能成员函数", "① <code>s.substr(pos, len)</code> 切片；② <code>s.find(sub)</code>（找不到等于 <code>string::npos</code>）；③ <code>s.insert(pos, str)</code>；④ <code>s.replace(pos, len, str)</code>。"},
                {"读取整行含空格字符串", "写 <code>getline(cin, s);</code>（若前面刚用过 <code>cin &gt;&gt; n;</code>，必须写成 <code>getline(cin &gt;&gt; ws, s);</code> 跳过残留换行！）。"}
            }, "",
            "与 <code>printf(\"%s\", s.c_str())</code> 混用输出 <code>string</code> 时千万记得加 <code>.c_str()</code> 转成 C 字符串指针！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <string>
using namespace std;

int main() {
    string s, pat;
    cin >> s >> pat;
    size_t pos = s.find(pat);
    cout << (pos == string::npos ? -1 : (int)pos) << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_6_3", "练习：string 替换子串所有匹配位置", "completion",
            "输入主串 s、目标子串 oldSub 和新串 newSub（均无空格），将 s 中全部 oldSub（从左到右不重叠）替换为 newSub 后输出。",
            "hello_cpp_cpp_world cpp rust", "hello_rust_rust_world",
            R"JSON([{"label":"多处替换","input":"hello_cpp_cpp_world cpp rust","expected":"hello_rust_rust_world"},{"label":"长度变化","input":"aaaa aa b","expected":"bb"}])JSON",
            R"CPP(#include <iostream>
#include <string>
using namespace std;
int main() {
    string s, o, nw;
    cin >> s >> o >> nw;
    // TODO: 循环使用 s.find(o, pos) 并 replace 替换所有出现
    cout << s << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <string>
using namespace std;
int main() {
    string s, o, nw;
    cin >> s >> o >> nw;
    size_t pos = 0;
    while ((pos = s.find(o, pos)) != string::npos) {
        s.replace(pos, o.size(), nw);
        pos += nw.size();
    }
    cout << s << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "6.4"; l.chapter_id = "ch6"; l.sort_order = 4;
        l.title = "6.4 map 键值对字典与频率映射";
        l.source_ref = "《算法笔记》6.4"; l.time_comp = "O(log n)"; l.space_comp = "O(n)";
        l.viz_type = "bst"; l.viz_label = "map<Key, Value> 按 Key 升序的红黑树键值表";
        l.tutorial_html = make_tut(
            "<strong>什么时候把普通散列数组换成 <code>map</code>？</strong>当键是<strong>字符串（人名/单词）</strong>、<strong>极大整数 (<code>10^9</code>)</strong> 或<strong>稀疏下标</strong>时，<code>map&lt;string, int&gt;</code> 自动帮你映射且按 Key 升序排列！",
            {
                {"两大访问技巧", "写计数统计直接用 <code>mp[key]++</code>；但如果仅仅是查询存不存在，用 <code>mp.count(key)</code> 或 <code>mp.find(key) != mp.end()</code>（因为直接读 <code>mp[key]</code> 若不存在会自动插一个默认值占内存！）。"}
            }, "",
            "<code>map</code> 遍历用范围循环 <code>for (auto&amp; [k, v] : mp)</code>（C++17 结构化绑定）清晰无比！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <map>
#include <string>
using namespace std;

int main() {
    int n; cin >> n;
    map<string, int> mp;
    string w;
    while (n--) { cin >> w; mp[w]++; }
    for (auto& [k, v] : mp) cout << k << " " << v << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_6_4", "练习：map 统计词频并找出现最多单词", "completion",
            "输入 n 个小写单词，找出出现次数最多的单词（如有多个并列输出字典序最小那个）及其次数。",
            "5\napple banana apple orange banana", "apple 2",
            R"JSON([{"label":"同频取字典序最小","input":"5\napple banana apple orange banana","expected":"apple 2"},{"label":"唯一高频","input":"4\nz a a b","expected":"a 2"}])JSON",
            R"CPP(#include <iostream>
#include <map>
#include <string>
using namespace std;
int main() {
    int n; cin >> n;
    map<string, int> mp;
    // TODO: 读入并统计频次，利用 map 按 key 升序特性找出频次最高者
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <map>
#include <string>
using namespace std;
int main() {
    int n; cin >> n;
    map<string, int> mp;
    for (int i = 0; i < n; ++i) { string w; cin >> w; mp[w]++; }
    string best_w; int best_c = -1;
    for (const auto& kv : mp) {
        if (kv.second > best_c) { best_c = kv.second; best_w = kv.first; }
    }
    cout << best_w << " " << best_c << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "6.5"; l.chapter_id = "ch6"; l.sort_order = 5;
        l.title = "6.5-6.7 queue / priority_queue / stack 容器适配器";
        l.source_ref = "《算法笔记》6.5-6.7"; l.time_comp = "O(1) / O(log n)"; l.space_comp = "O(n)";
        l.viz_type = "stack"; l.viz_label = "LIFO 栈、FIFO 队列与小根优先队列行为对比";
        l.tutorial_html = make_tut(
            "<strong>三个手写数据结构可以直接省略 30 行数组模拟代码的神器：</strong><code>stack</code>（DFS/括号）、<code>queue</code>（BFS 层序）、<code>priority_queue</code>（小根堆优化 Dijkstra、合并果子）！",
            {
                {"小根堆标准声明", "<code>priority_queue&lt;int, vector&lt;int&gt;, greater&lt;int&gt;&gt; pq;</code>（默认不写后两个参数是大根堆！）。"},
                {"自定义结构体堆优先级", "记准<strong>优先队列比较符号和 sort 相反</strong>：<code>bool operator&lt;(const Node&amp; o) const { return d &gt; o.d; }</code> 表示距离 <code>d</code> 小的放堆顶！"}
            }, "",
            "每次调用 <code>q.front()</code> / <code>s.top()</code> / <code>pq.top()</code> 和 <code>pop()</code> 之前务必确保 <code>!empty()</code>，空容器取值直接未定义崩溃！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <queue>
#include <vector>
using namespace std;

int main() {
    priority_queue<long long, vector<long long>, greater<long long>> pq;
    int n; cin >> n;
    for (int i = 0; i < n; ++i) { long long x; cin >> x; pq.push(x); }
    long long ans = 0;
    while (pq.size() > 1) {
        long long a = pq.top(); pq.pop();
        long long b = pq.top(); pq.pop();
        ans += a + b;
        pq.push(a + b);
    }
    cout << ans << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_6_5", "练习：小根 priority_queue 合并果子", "completion",
            "输入 n 堆果子重量，每次合并最轻的两堆，求合成 1 堆所需的最小总体力消耗。",
            "3\n1 2 9", "15",
            R"JSON([{"label":"合并果子样例","input":"3\n1 2 9","expected":"15"},{"label":"四堆","input":"4\n1 2 3 4","expected":"19"}])JSON",
            R"CPP(#include <iostream>
#include <queue>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    priority_queue<long long, vector<long long>, greater<long long>> pq;
    for (int i = 0; i < n; ++i) { long long x; cin >> x; pq.push(x); }
    long long cost = 0;
    // TODO: 反复取堆顶最小两堆求和累加并放回
    cout << cost << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <queue>
#include <vector>
using namespace std;
int main() {
    int n; cin >> n;
    priority_queue<long long, vector<long long>, greater<long long>> pq;
    for (int i = 0; i < n; ++i) { long long x; cin >> x; pq.push(x); }
    long long cost = 0;
    while (pq.size() > 1) {
        long long a = pq.top(); pq.pop();
        long long b = pq.top(); pq.pop();
        cost += a + b;
        pq.push(a + b);
    }
    cout << cost << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "6.8"; l.chapter_id = "ch6"; l.sort_order = 6;
        l.title = "6.8-6.9 pair 二元组与 <algorithm> 高频函数全集";
        l.source_ref = "《算法笔记》6.8-6.9 · 前导 PPT 点名"; l.time_comp = "O(n log n)"; l.space_comp = "O(1)";
        l.viz_type = "bsearch"; l.viz_label = "lower_bound 与 upper_bound 在有序段二分左右界";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》6.9 把最实用的算法全打包在这里：</strong><code>max/min/abs/swap/reverse/fill/sort/next_permutation/lower_bound/upper_bound</code>，熟练以后代码量直接减半！",
            {
                {"pair 两用", "<code>pair&lt;int, int&gt; p = {x, y};</code> 自带按先 <code>first</code> 后 <code>second</code> 比较，懒得写 <code>struct</code> 时首选！"},
                {"lower_bound & upper_bound", "有序数组内：<code>lower_bound(a, a+n, x)</code> 找首个 <code>&gt;= x</code> 指针，<code>upper_bound</code> 找首个 <code>&gt; x</code> 指针，相减即得 <code>x</code> 出现次数！"},
                {"next_permutation", "用前必须先排序成最小字典序，配合 <code>do { ... } while (next_permutation(...))</code> 枚举全排列。"}
            }, "",
            "<code>memset</code> 只用来清零或赋 -1/INF；若想把数组全初始化成特定整数如 <code>999</code>，务必使用 <code>fill(a, a + n, 999);</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n, q; cin >> n >> q;
    vector<int> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    sort(a.begin(), a.end());
    while (q--) {
        int x; cin >> x;
        int cnt = (int)(upper_bound(a.begin(), a.end(), x) - lower_bound(a.begin(), a.end(), x));
        cout << cnt << "\n";
    }
    return 0;
})CPP";
        add_ex(l, "ex_6_8", "练习：lower_bound/upper_bound 二分统计次数", "completion",
            "输入 n 个整数及 q 次查询，每次查询整数 x 在数组中出现的精确次数（要求排序 + 二分复杂度）。",
            "6 2\n4 1 2 4 4 9\n4\n5", "3\n0",
            R"JSON([{"label":"二分计数","input":"6 2\n4 1 2 4 4 9\n4\n5","expected":"3\n0"},{"label":"全部相等","input":"3 1\n7 7 7\n7","expected":"3"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n, q; cin >> n >> q;
    vector<int> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    sort(a.begin(), a.end());
    while (q--) {
        int x; cin >> x;
        // TODO: 用 upper_bound - lower_bound 输出 x 的个数
        cout << 0 << "\n";
    }
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;
int main() {
    int n, q; cin >> n >> q;
    vector<int> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    sort(a.begin(), a.end());
    while (q--) {
        int x; cin >> x;
        int cnt = (int)(upper_bound(a.begin(), a.end(), x) - lower_bound(a.begin(), a.end(), x));
        cout << cnt << "\n";
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }
}

} // namespace ds_seed
} // namespace omni
