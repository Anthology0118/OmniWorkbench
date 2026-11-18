#pragma once

#include "ds_seed_common.hpp"

namespace omni {
namespace ds_seed {

inline void seed_ch0_ch3(std::vector<DsChapter>& chapters, std::vector<DsLesson>& lessons) {
    // =========================================================================
    // 第 0 章｜算法集训课程前导（0.算法集训-课程前导.pptx）
    // =========================================================================
    chapters.push_back({"ch0", 0, "C语言入门(Ch0-3)", "第 0 章｜集训前导：课程目标与复杂度估算", "前导 PPT P1-P12 · 洛谷/LC · 1s≈10^8"});

    {
        DsLesson l; l.id = "0.1"; l.chapter_id = "ch0"; l.sort_order = 1;
        l.title = "0.1 集训目标、前置路线与科学刷题闭环";
        l.source_ref = "前导 PPT P1-P8"; l.time_comp = "—"; l.space_comp = "—";
        l.viz_type = "complexity"; l.viz_label = "先学 C 语言基础，再转 C++ STL 与数据结构";
        l.tutorial_html = make_tut(
            "<strong>学习路线全景：</strong>先通过第 1–5 章掌握 <strong>纯 C 语言语法（输入输出、数组、指针、结构体）</strong>与入门模拟/数学；再在第 6 章正式接入 <strong>C++ STL 标准模板库</strong>，最终完成第 7–13 章树、图、DP 与高阶数据结构训练。",
            {
                {"三步刷题习惯", "① 先在本地练习框写代码并自测通过样例，再提交洛谷 / LeetCode；② 切忌抄题解，看懂思路后关掉题解重写；③ 错题写简短复盘（错因+正解+模板）。"},
                {"语言衔接特点", "C++ 完全兼容绝大多数 C 语法（如 <code>scanf/printf</code>、<code>struct</code>、<code>char[]</code>），前期用 C 打牢内存与指针直觉，后期叠加 C++ 引用与 STL 效率翻倍！"}
            }, "",
            "遇到 <code>=</code> 和 <code>==</code> 混用、数组越界、未初始化局部变量等低级语法错误时，养成先开启 <code>-Wall</code> 看编译警告的好习惯。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    long long a = 0, b = 0;
    if (scanf("%lld %lld", &a, &b) == 2) {
        printf("%lld\n", a + b);
    }
    return 0;
})CPP";
        add_ex(l, "ex_0_1", "练习：C 语言标准 64 位整数 A+B", "completion",
            "使用 <stdio.h> 与 scanf/printf 读取两个长整型 (%lld)，输出其精确和。",
            "1000000000000 2000000000000", "3000000000000",
            R"JSON([{"label":"64位求和","input":"1000000000000 2000000000000","expected":"3000000000000"},{"label":"正负抵消","input":"-42 100","expected":"58"}])JSON",
            R"CPP(#include <stdio.h>

int main() {
    long long a = 0, b = 0;
    // TODO: 使用 scanf 读取 %lld 并用 printf 输出 a + b
    printf("%lld\n", 0LL);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>

int main() {
    long long a = 0, b = 0;
    if (scanf("%lld %lld", &a, &b) == 2) {
        printf("%lld\n", a + b);
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "0.2"; l.chapter_id = "ch0"; l.sort_order = 2;
        l.title = "0.2 时间复杂度反推算法与 1s = 10^8 法则";
        l.source_ref = "前导 PPT P10-P11"; l.time_comp = "O(1)~O(2^n)"; l.space_comp = "O(n)";
        l.viz_type = "complexity"; l.viz_label = "数据规模 n 决定允许的算法时间复杂度";
        l.tutorial_html = make_tut(
            "<strong>前导课强调的核心技巧：</strong>评测机 1 秒大约可执行 <code>10^7 ~ 10^8</code> 次基本指令。动笔前先瞄一眼数据范围 <code>n</code>，就能反推出能过关的算法！",
            {
                {"O(1) ~ O(log n)", "<code>n ≥ 10^9</code>：只能用数学公式、快速幂或二分查找。"},
                {"O(n) ~ O(n log n)", "<code>n ≤ 10^5 ~ 10^6</code>：排序、双指针、前缀和、堆、Dijkstra、树状数组。"},
                {"O(n^2) ~ O(n^3) / 指数", "<code>n ≤ 5000</code> 许用平方；<code>n ≤ 500</code> 许用立方 (Floyd)；<code>n ≤ 20</code> 状压 DP / DFS 搜索。"}
            }, "",
            "计算 <code>n*(n+1)/2</code> 时若先乘后除，<code>n = 10^9</code> 在 <code>int</code> 下直接溢出；甚至 <code>n = 2×10^9</code> 时即使 <code>long long</code> 也先除 2 再乘防溢出。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    long long n;
    scanf("%lld", &n);
    long long ans = (n % 2 == 0) ? (n / 2) * (n + 1) : ((n + 1) / 2) * n;
    printf("%lld\n", ans);
    return 0;
})CPP";
        add_ex(l, "ex_0_2", "练习：O(1) 求解 1..n 等差数列和", "completion",
            "输入 n (1 <= n <= 10^9)，用 O(1) 公式计算 1+2+...+n 并输出。",
            "1000000000", "500000000500000000",
            R"JSON([{"label":"十亿求和","input":"1000000000","expected":"500000000500000000"},{"label":"小规模","input":"10","expected":"55"}])JSON",
            R"CPP(#include <stdio.h>

int main() {
    long long n;
    scanf("%lld", &n);
    long long ans = 0;
    // TODO: O(1) 计算累加和
    printf("%lld\n", ans);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>

int main() {
    long long n;
    scanf("%lld", &n);
    long long ans = (n % 2 == 0) ? (n / 2) * (n + 1) : ((n + 1) / 2) * n;
    printf("%lld\n", ans);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "0.3"; l.chapter_id = "ch0"; l.sort_order = 3;
        l.title = "0.3 前导指定例题：洛谷 P1003 铺地毯 & LC59 螺旋矩阵";
        l.source_ref = "前导 PPT P12 课上例题"; l.time_comp = "O(n) / O(n^2)"; l.space_comp = "O(n)";
        l.viz_type = "two_ptr"; l.viz_label = "逆序扫描矩形覆盖 & 螺旋方向转向";
        l.tutorial_html = make_tut(
            "<strong>前导 PPT 最后一页点名必练：</strong>① <strong>洛谷 P1003 铺地毯</strong> 存下每张地毯参数后<strong>倒序</strong>检查首个覆盖 <code>(x,y)</code> 的地毯；② <strong>LeetCode 59 螺旋矩阵 II</strong> 利用方向数组顺时针转向填入。先用纯 C 数组搞定！",
            {
                {"P1003 逆向思维", "只问最上方那张地毯 → 倒序遍历 <code>i = n-1 .. 0</code>，首个满足 <code>a[i]<=x && x<=a[i]+g[i] && b[i]<=y && y<=b[i]+k[i]</code> 即为 <code>i + 1</code>。"},
                {"LC59 方向数组", "<code>dr[4]={0,1,0,-1}, dc[4]={1,0,-1,0}</code>，遇到越界或已填充格则 <code>d = (d + 1) % 4</code>。"}
            }, "",
            "P1003 边长范围 <code>10^5</code> 切勿开二维数组模拟填色，必定 MLE + TLE！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int a[10005], b[10005], g[10005], k[10005];
int main() {
    int n, x, y, ans = -1;
    scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d%d%d%d", &a[i], &b[i], &g[i], &k[i]);
    scanf("%d%d", &x, &y);
    for (int i = n - 1; i >= 0; --i) {
        if (x >= a[i] && x <= a[i] + g[i] && y >= b[i] && y <= b[i] + k[i]) { ans = i + 1; break; }
    }
    printf("%d\n", ans);
    return 0;
})CPP";
        add_ex(l, "ex_0_3_a", "练习 1：洛谷 P1003 铺地毯 (纯 C 实现)", "completion",
            "输入 n 张地毯 (a, b, g, k) 及查询坐标 (x, y)，输出最上方地毯编号（无覆盖输出 -1）。",
            "3\n1 0 2 3\n0 2 3 3\n2 1 3 3\n2 2", "3",
            R"JSON([{"label":"洛谷 P1003","input":"3\n1 0 2 3\n0 2 3 3\n2 1 3 3\n2 2","expected":"3"},{"label":"无地毯覆盖","input":"2\n0 0 2 2\n1 1 2 2\n5 5","expected":"-1"}])JSON",
            R"CPP(#include <stdio.h>
int a[10005], b[10005], g[10005], k[10005];
int main() {
    int n, x, y, ans = -1;
    scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d%d%d%d", &a[i], &b[i], &g[i], &k[i]);
    scanf("%d%d", &x, &y);
    // TODO: 倒序查找最上方覆盖 (x, y) 的地毯编号
    printf("%d\n", ans);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int a[10005], b[10005], g[10005], k[10005];
int main() {
    int n, x, y, ans = -1;
    scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d%d%d%d", &a[i], &b[i], &g[i], &k[i]);
    scanf("%d%d", &x, &y);
    for (int i = n - 1; i >= 0; --i) {
        if (x >= a[i] && x <= a[i] + g[i] && y >= b[i] && y <= b[i] + k[i]) {
            ans = i + 1;
            break;
        }
    }
    printf("%d\n", ans);
    return 0;
})CPP");
        add_ex(l, "ex_0_3_b", "练习 2：LeetCode 59 螺旋矩阵 II", "completion",
            "输入 n (1 <= n <= 15)，顺时针生成 1..n^2 的螺旋方阵并按行输出。",
            "3", "1 2 3\n8 9 4\n7 6 5",
            R"JSON([{"label":"n=3 方阵","input":"3","expected":"1 2 3\n8 9 4\n7 6 5"},{"label":"n=1","input":"1","expected":"1"}])JSON",
            R"CPP(#include <stdio.h>
int mat[20][20];
int main() {
    int n; scanf("%d", &n);
    int dr[4] = {0, 1, 0, -1}, dc[4] = {1, 0, -1, 0};
    int r = 0, c = 0, d = 0;
    for (int v = 1; v <= n * n; ++v) {
        mat[r][c] = v;
        // TODO: 计算 nr, nc 并判断越界/已填以转向
    }
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) printf("%s%d", j ? " " : "", mat[i][j]);
        printf("\n");
    }
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int mat[20][20];
int main() {
    int n; scanf("%d", &n);
    int dr[4] = {0, 1, 0, -1}, dc[4] = {1, 0, -1, 0};
    int r = 0, c = 0, d = 0;
    for (int v = 1; v <= n * n; ++v) {
        mat[r][c] = v;
        int nr = r + dr[d], nc = c + dc[d];
        if (nr < 0 || nr >= n || nc < 0 || nc >= n || mat[nr][nc] != 0) {
            d = (d + 1) % 4;
            nr = r + dr[d]; nc = c + dc[d];
        }
        r = nr; c = nc;
    }
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) printf("%s%d", j ? " " : "", mat[i][j]);
        printf("\n");
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 1 章｜如何使用本书（《算法笔记》Ch1 全部 1.1-1.5）
    // =========================================================================
    chapters.push_back({"ch1", 1, "C语言入门(Ch0-3)", "第 1 章｜如何使用本书与 OJ 评测体系", "《算法笔记》Ch1 · 1.1-1.5 全覆盖"});

    {
        DsLesson l; l.id = "1.1"; l.chapter_id = "ch1"; l.sort_order = 1;
        l.title = "1.1-1.2 本书内容安排与 C / C++ 编译器选择";
        l.source_ref = "《算法笔记》1.1-1.2"; l.time_comp = "—"; l.space_comp = "—";
        l.viz_type = "complexity"; l.viz_label = "全书脉络：C语言快速入门 → 入门算法 → C++ STL → 提高篇数据结构";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》的核心定位：</strong>全书前半部分（第 2–5 章）主要以<strong>纯 C 语言语法</strong>（<code>#include &lt;stdio.h&gt;</code>、<code>scanf/printf</code>、指针、结构体）讲解，同时平滑引出 C++ 引用和 <code>cin/cout</code>；到第 6 章正式引入 <strong>C++ STL 容器与算法</strong>，使编程效率倍增。",
            {
                {"为何选 C++ 编译器写 C 风格代码？", "C++ 文件扩展名为 <code>.cpp</code>，兼容几乎全部 C 语言语法及 <code>stdio.h</code>/<code>string.h</code>/<code>math.h</code>，同时让你在需要 <code>sort</code>、<code>vector</code>、<code>map</code> 时能零成本使用。"},
                {"全书三大篇结构", "<strong>快速入门篇</strong>（Ch2）补齐 C 语言与 C++ 过渡语法；<strong>入门模拟与初阶算法篇</strong>（Ch3–Ch5）打牢模拟、排序、贪心、二分、数论；<strong>提高篇</strong>（Ch6–Ch13）覆盖 STL、链表/栈队列、搜索、树图、DP、KMP、树状数组。"}
            }, "",
            "在旧版 C/C++ 考试环境（如部分高校机试）若使用 C++11/17 编译，建议使用 <code>#include &lt;cstdio&gt;</code> / <code>#include &lt;stdio.h&gt;</code>，避免非标准的 <code>gets()</code> 函数（已被 C11 移除，改用 <code>fgets</code> 或逐字符读取）。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

// 既符合传统 C 语言习惯，又可无缝在 C++ 环境下编译运行
int main() {
    printf("Ready for Algorithm Notes!\n");
    return 0;
})CPP";
        add_ex(l, "ex_1_1", "练习：标准 C 格式化问候输出", "completion",
            "读取一个整型年份 y，使用 printf 输出 \"Algorithm 2026\" 格式的文本（将 2026 替换为读入的 y）。",
            "2026", "Algorithm 2026",
            R"JSON([{"label":"输出年份","input":"2026","expected":"Algorithm 2026"},{"label":"2025","input":"2025","expected":"Algorithm 2025"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    int y = 0;
    scanf("%d", &y);
    // TODO: 按格式输出 Algorithm <y>
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    int y = 0;
    scanf("%d", &y);
    printf("Algorithm %d\n", y);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "1.3"; l.chapter_id = "ch1"; l.sort_order = 2;
        l.title = "1.3-1.5 在线评测系统 (OJ)、八种常见结果与高效做题";
        l.source_ref = "《算法笔记》1.3-1.5"; l.time_comp = "—"; l.space_comp = "—";
        l.viz_type = "complexity"; l.viz_label = "OJ 评测机制：黑盒对比标准输入与标准输出";
        l.tutorial_html = make_tut(
            "<strong>什么是 OJ 黑盒评测？</strong>系统只把测试数据文件重定向进你的程序 <code>stdin</code>，并把你的 <code>stdout</code> 与标准答案逐字符比对！因此<strong>千万不要输出「请输入 n：」等提示语</strong>，否则必吃 WA！",
            {
                {"八大常见评测结果完全解读", "<strong>AC</strong>(通过) / <strong>WA</strong>(答案错) / <strong>TLE</strong>(超时) / <strong>MLE</strong>(超内存) / <strong>RE</strong>(运行崩：除零/越界/爆栈) / <strong>PE</strong>(格式多空格空行) / <strong>CE</strong>(编译失败) / <strong>OLE</strong>(输出过多)。"},
                {"高效刷题 4 个好习惯", "① 自己造极限边界样例（0、1、最大值、全相等）；② 少用全局变量名如 <code>y1</code>、<code>next</code> 防和库函数重名；③ 调试输出提交前务必删除。"}
            },
            "<div class=\"ds-tut-table-wrap\"><table class=\"ds-tut-table\"><thead><tr><th>缩写</th><th>含义</th><th>核心排错第一动作</th></tr></thead>"
            "<tbody><tr><td><span class=\"ds-tag red\">WA</span></td><td>答案错误</td><td>检查数据范围是否溢出 <code>int</code>、循环上下界差 1</td></tr>"
            "<tr><td><span class=\"ds-tag amber\">TLE</span></td><td>超时</td><td>查死循环 + 估复杂度是否超过 <code>10^8</code></td></tr>"
            "<tr><td><span class=\"ds-tag purple\">RE</span></td><td>运行时错误</td><td>查数组下标负值/越界、除以 0、无穷递归</td></tr>"
            "<tr><td><span class=\"ds-tag blue\">PE</span></td><td>格式错误</td><td>检查行末多余空格与换行符数量</td></tr></tbody></table></div>",
            "在洛谷/PTA 严格评测中，行末多打一个空格在部分古早 OJ 上会判 PE/WA！建议用 <code>if (i > 0) printf(\" \");</code> 控制元素间空格。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

// 严谨控制行末无多余空格的标准输出写法
int main() {
    int n;
    scanf("%d", &n);
    for (int i = 1; i <= n; ++i) {
        if (i > 1) printf(" ");
        printf("%d", i);
    }
    printf("\n");
    return 0;
})CPP";
        add_ex(l, "ex_1_3", "练习：无多余空格规范化输出", "completion",
            "输入正整数 n，按行输出 1 到 n 的偶数序列，要求两两之间一个空格、行末无多余空格。",
            "7", "2 4 6",
            R"JSON([{"label":"常规测试","input":"7","expected":"2 4 6"},{"label":"单偶数","input":"2","expected":"2"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    int n, first = 1;
    scanf("%d", &n);
    // TODO: 严格控制元素间空格输出 1..n 中的偶数
    printf("\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    int n, first = 1;
    scanf("%d", &n);
    for (int i = 2; i <= n; i += 2) {
        if (!first) printf(" ");
        printf("%d", i);
        first = 0;
    }
    printf("\n");
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 2 章｜C/C++ 快速入门（C 语言完整语法 + C++ 进阶过渡，全 11 节）
    // =========================================================================
    chapters.push_back({"ch2", 2, "C语言入门(Ch0-3)", "第 2 章｜C 语言完整入门到 C++ 过渡（全 11 节）", "《算法笔记》Ch2 · 2.1-2.10 全小节"});

    {
        DsLesson l; l.id = "2.1"; l.chapter_id = "ch2"; l.sort_order = 1;
        l.title = "2.1 基本数据类型、强转、宏/const 与位运算符";
        l.source_ref = "《算法笔记》2.1.1-2.1.5"; l.time_comp = "O(1)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "int / long long / double / char 类型与位运算";
        l.tutorial_html = make_tut(
            "<strong>纯 C 数据类型基石：</strong>整数首选 <code>int</code>（32 位，最大约 <code>2.14×10^9</code>）与 <code>long long</code>（64 位，最大约 <code>9.22×10^18</code>）；浮点千万少用 <code>float</code>（有效数字仅 6–7 位），一律用 <strong><code>double</code></strong>（15–16 位有效精度）。",
            {
                {"强制类型转换与整除陷阱", "<code>(double)a / b</code> 才会得到小数；两个 <code>int</code> 直接写 <code>5 / 2</code> 结果截断为 <code>2</code>。"},
                {"#define 宏与 const 常量", "<code>#define INF 0x3fffffff</code>（注意末尾<strong>不加分号</strong>，复杂表达式务必加括号）；推荐使用类型安全的 <code>const double PI = acos(-1.0);</code>。"},
                {"位运算提速技巧", "<code>x &amp; 1</code> 判奇偶、<code>x &lt;&lt; 1</code> 乘 2、<code>x &gt;&gt; 1</code> 除 2（注意位运算优先级低于 <code>==</code>，记得写 <code>(x &amp; 1) == 1</code>）。"}
            }, "",
            "宏替换是纯文本替换！例如 <code>#define ADD(a, b) a + b</code>，写成 <code>ADD(1, 2) * 3</code> 会展开成 <code>1 + 2 * 3 = 7</code>！宏参和整个宏体都要加括号。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    int a, b;
    scanf("%d %d", &a, &b);
    printf("div=%.2f mod=%d xor=%d\n", (double)a / b, a % b, a ^ b);
    return 0;
})CPP";
        add_ex(l, "ex_2_1", "练习：强转保留两位小数与异或位运算", "completion",
            "读入两个正整数 a, b，第一行输出 (double)a / b 保留两位小数，第二行输出 (a ^ b) 的十进制值。",
            "7 2", "3.50\n5",
            R"JSON([{"label":"小数与异或","input":"7 2","expected":"3.50\n5"},{"label":"整除","input":"10 5","expected":"2.00\n15"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    int a, b;
    scanf("%d %d", &a, &b);
    // TODO: 强转为 double 输出两位小数 %.2f，下一行输出 a ^ b
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    int a, b;
    scanf("%d %d", &a, &b);
    printf("%.2f\n%d\n", (double)a / b, a ^ b);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.2"; l.chapter_id = "ch2"; l.sort_order = 2;
        l.title = "2.2 scanf/printf、getchar/putchar、typedef 与 math.h";
        l.source_ref = "《算法笔记》2.2.1-2.2.6"; l.time_comp = "O(1)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "C 格式控制符 %d %lld %f %c %s 与补零 %02d";
        l.tutorial_html = make_tut(
            "<strong>纯 C 格式化输入输出核心对照：</strong>读 <code>double</code> 在 <code>scanf</code> 里<strong>必须用 <code>%lf</code></strong>，而 <code>printf</code> 里 <code>%f</code> 即可；<code>%02d</code> 用于日期时间补零对齐（如 <code>09</code> 月 <code>05</code> 日）。",
            {
                {"scanf 格式细节与空白符", "<code>scanf(\"%c\", &amp;c)</code> 会把上一次输入留下的换行符或空格当字符读入！可在前面加空格 <code>scanf(\" %c\", &amp;c)</code> 自动吃掉空白符，或用 <code>getchar()</code> 吞掉换行。"},
                {"typedef 起别名", "<code>typedef long long LL;</code> 是缩短代码长度的竞赛标准写法。"},
                {"&lt;math.h&gt; 常用函数", "<code>fabs(x)</code> 浮点绝对值、<code>sqrt(x)</code> 开方、<code>pow(a, b)</code> 浮点乘方、<code>floor/ceil/round</code> 下上取整与四舍五入。"}
            }, "",
            "绝对值函数注意区分：整数绝对值用 <code>abs(x)</code>（<code>&lt;stdlib.h&gt;</code>），浮点数绝对值必须用 <code>fabs(x)</code>（<code>&lt;math.h&gt;</code>），混用会丢精度！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <math.h>

typedef long long LL;

int main() {
    int hh, mm;
    scanf("%d:%d", &hh, &mm);
    printf("%02d:%02d %.2f\n", hh, mm, sqrt((double)(hh * hh + mm * mm)));
    return 0;
})CPP";
        add_ex(l, "ex_2_2", "练习：scanf 按冒号拆分时间并补零输出", "completion",
            "输入形如 \"9:5\" 的时间，用 scanf(\"%d:%d\") 解析并按标准两位数补零 \"09:05\" 输出，第二行输出 sqrt(hh*hh + mm*mm) 保留 2 位小数。",
            "3:4", "03:04\n5.00",
            R"JSON([{"label":"补零与开方","input":"3:4","expected":"03:04\n5.00"},{"label":"整点","input":"12:0","expected":"12:00\n12.00"}])JSON",
            R"CPP(#include <stdio.h>
#include <math.h>
int main() {
    int hh, mm;
    // TODO: 解析 hh:mm 并用 %02d 输出时间，下行为 sqrt 结果 %.2f
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <math.h>
int main() {
    int hh, mm;
    scanf("%d:%d", &hh, &mm);
    printf("%02d:%02d\n%.2f\n", hh, mm, sqrt((double)(hh * hh + mm * mm)));
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.3"; l.chapter_id = "ch2"; l.sort_order = 3;
        l.title = "2.3 选择结构 (if-else 嵌套与 switch-case 分支)";
        l.source_ref = "《算法笔记》2.3.1-2.3.3"; l.time_comp = "O(1)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "条件判断分支与 switch break 穿透陷阱";
        l.tutorial_html = make_tut(
            "<strong>条件分支核心规范：</strong>① <code>if (n != 0)</code> 可简写 <code>if (n)</code>；② 多重 <code>else if</code> 按互斥区间从上往下筛查；③ 多值等值枚举用 <code>switch (x) { case ...: ... break; default: ... }</code>。",
            {
                {"闰年判断一行公式", "能被 400 整除，或能被 4 整除且不能被 100 整除：<code>(y % 400 == 0) || (y % 4 == 0 &amp;&amp; y % 100 != 0)</code>。"},
                {"switch 的贯穿 (Fall-through) 陷阱", "每个 <code>case</code> 结尾必须加 <code>break;</code>，否则程序会顺着执行下一个 <code>case</code> 直到遇到 <code>break</code>！"}
            }, "",
            "判断区间千万别写成数学连等式 <code>10 &lt;= x &lt;= 20</code>！C 语言中 <code>10 &lt;= x</code> 先算出 <code>0</code> 或 <code>1</code>，再和 <code>20</code> 比永远为真！必须写成 <code>x &gt;= 10 &amp;&amp; x &lt;= 20</code>。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    int y; scanf("%d", &y);
    int is_leap = (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0);
    printf("%s\n", is_leap ? "YES" : "NO");
    return 0;
})CPP";
        add_ex(l, "ex_2_3", "练习：成绩分档（switch-case / 区间判断）", "completion",
            "输入百分制成绩 s (0..100)：>=90 输出 A，80~89 输出 B，70~79 输出 C，60~69 输出 D，<60 输出 F。",
            "85", "B",
            R"JSON([{"label":"85分->B","input":"85","expected":"B"},{"label":"100分->A","input":"100","expected":"A"},{"label":"59分->F","input":"59","expected":"F"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    int s; scanf("%d", &s);
    // TODO: 判断分数等级并输出 A/B/C/D/F
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    int s; scanf("%d", &s);
    if (s >= 90) printf("A\n");
    else if (s >= 80) printf("B\n");
    else if (s >= 70) printf("C\n");
    else if (s >= 60) printf("D\n");
    else printf("F\n");
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.4"; l.chapter_id = "ch2"; l.sort_order = 4;
        l.title = "2.4 循环结构 (while / do-while / for / break / continue)";
        l.source_ref = "《算法笔记》2.4.1-2.4.4"; l.time_comp = "O(n)"; l.space_comp = "O(1)";
        l.viz_type = "two_ptr"; l.viz_label = "三类循环控制流与 break / continue 跳转";
        l.tutorial_html = make_tut(
            "<strong>三大循环使用分工：</strong>已知次数循环用 <code>for</code>；条件满足一直循环用 <code>while</code>；<strong>至少先执行一次</strong>（如拆解整数各位数、进制转换）用 <code>do { ... } while (...);</code>。",
            {
                {"拆解整数各位数（必背 C 模板）", "<code>do { sum += n % 10; n /= 10; } while (n > 0);</code>（当 <code>n = 0</code> 也能正确走一轮）。"},
                {"break vs continue", "<code>break</code> 直接跳出整层循环；<code>continue</code> 跳过本轮剩下语句直接进入下一轮判断/增量步。"}
            }, "",
            "在 <code>for (int i = 0; i &lt; n; ++i)</code> 分号后面千万不要误加多余分号 <code>;</code>，否则循环体变成空语句！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    int n, sum = 0;
    scanf("%d", &n);
    do {
        sum += n % 10;
        n /= 10;
    } while (n > 0);
    printf("%d\n", sum);
    return 0;
})CPP";
        add_ex(l, "ex_2_4", "练习：计算整数各位数字之和（do-while）", "completion",
            "输入非负整数 n，用循环拆位求各位数字之和并输出。",
            "12345", "15",
            R"JSON([{"label":"五位数","input":"12345","expected":"15"},{"label":"零边界","input":"0","expected":"0"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    long long n, sum = 0;
    scanf("%lld", &n);
    // TODO: 拆解 n 的每一位累加到 sum
    printf("%lld\n", sum);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    long long n, sum = 0;
    scanf("%lld", &n);
    do {
        sum += n % 10;
        n /= 10;
    } while (n > 0);
    printf("%lld\n", sum);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.5A"; l.chapter_id = "ch2"; l.sort_order = 5;
        l.title = "2.5A 一维/二维数组、冒泡排序与 memset 赋初值";
        l.source_ref = "《算法笔记》2.5.1-2.5.4"; l.time_comp = "O(n^2)"; l.space_comp = "O(n)";
        l.viz_type = "two_ptr"; l.viz_label = "冒泡排序每趟把最大值沉入数组尾部";
        l.tutorial_html = make_tut(
            "<strong>数组开空间与初值黄金法则：</strong>超过 <code>10^5</code> 的大数组必须声明在 <code>main</code> <strong>外边（全局区）</strong>，既免 RE 爆栈又默认全清零；局部数组一定要手动初始化或用 <code>memset(a, 0, sizeof(a))</code>。",
            {
                {"手写冒泡排序（C 基础必背）", "共做 <code>n - 1</code> 趟，第 <code>i</code> 趟将相邻逆序对交换沉底，时间 <code>O(n^2)</code>（加 <code>swapped</code> 标志可提前退出）。"},
                {"memset 按字节赋值原理", "<code>memset(arr, val, sizeof(arr))</code> 把每个<strong>字节</strong>刷成 <code>val</code>：只适合赋 <code>0</code>、<code>-1</code> 或 <code>0x3f</code>（<code>0x3f3f3f3f</code> 无穷大）。"}
            }, "",
            "绝对不能对整型数组写 <code>memset(a, 1, sizeof(a))</code>（结果每个 int 会变成 <code>16843009</code> 而非 <code>1</code>）！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <string.h>

int a[1005];
int main() {
    memset(a, 0, sizeof(a));
    int n; scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - 1 - i; ++j) {
            if (a[j] > a[j + 1]) {
                int tmp = a[j]; a[j] = a[j + 1]; a[j + 1] = tmp;
            }
        }
    }
    for (int i = 0; i < n; ++i) printf("%s%d", i ? " " : "", a[i]);
    printf("\n");
    return 0;
})CPP";
        add_ex(l, "ex_2_5_a", "练习：C 语言手写冒泡排序", "completion",
            "不借助 C++ sort，手写冒泡排序将输入的 n 个整数按升序排列并输出。",
            "5\n5 1 4 2 8", "1 2 4 5 8",
            R"JSON([{"label":"冒泡排序","input":"5\n5 1 4 2 8","expected":"1 2 4 5 8"},{"label":"单元素","input":"1\n9","expected":"9"}])JSON",
            R"CPP(#include <stdio.h>
int a[1005];
int main() {
    int n; scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    // TODO: 实现冒泡排序二重循环
    for (int i = 0; i < n; ++i) printf("%s%d", i ? " " : "", a[i]);
    printf("\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int a[1005];
int main() {
    int n; scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    for (int i = 0; i < n - 1; ++i) {
        for (int j = 0; j < n - 1 - i; ++j) {
            if (a[j] > a[j + 1]) {
                int t = a[j]; a[j] = a[j + 1]; a[j + 1] = t;
            }
        }
    }
    for (int i = 0; i < n; ++i) printf("%s%d", i ? " " : "", a[i]);
    printf("\n");
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.5B"; l.chapter_id = "ch2"; l.sort_order = 6;
        l.title = "2.5B C 字符数组 char[]、string.h 与 sscanf/sprintf";
        l.source_ref = "《算法笔记》2.5.5-2.5.7"; l.time_comp = "O(L)"; l.space_comp = "O(L)";
        l.viz_type = "two_ptr"; l.viz_label = "C 字符串以 '\\0' (ASCII 0) 结尾";
        l.tutorial_html = make_tut(
            "<strong>在进入 C++ <code>std::string</code> 前必须掌握的纯 C 字符串底座：</strong>C 串是字符数组末尾带结束符 <code>'\\0'</code>（开数组记得多预留 1 位），用 <code>&lt;string.h&gt;</code> 操作；而 <code>sscanf / sprintf</code> 是处理复杂字符串格式解析的超级利器！",
            {
                {"string.h 四大函数", "<code>strlen(s)</code> 长度、<code>strcmp(a, b)</code> 字典序比大小（返回负数/0/正数）、<code>strcpy(dst, src)</code> 复制、<code>strcat(dst, src)</code> 拼接。"},
                {"sscanf 与 sprintf", "<code>sscanf(str, \"%d:%lf\", &amp;n, &amp;x)</code> 从<strong>内存字符串</strong>里按格式提取数据；<code>sprintf(str, \"%04d\", n)</code> 把数据按格式写回字符串！"}
            }, "",
            "在 <code>for (int i = 0; i &lt; strlen(s); ++i)</code> 循环条件中每次都调用 <code>strlen(s)</code> 会让复杂度从 <code>O(L)</code> 恶化为 <code>O(L^2)</code>！务必先用 <code>int len = strlen(s);</code> 存好长度！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <string.h>

int main() {
    char buf[128] = "123:45.67";
    int n = 0; double x = 0.0;
    sscanf(buf, "%d:%lf", &n, &x);
    char out[128];
    sprintf(out, "n=%d x=%.1f len=%zu", n, x, strlen(buf));
    printf("%s\n", out);
    return 0;
})CPP";
        add_ex(l, "ex_2_5_b", "练习：sscanf 解析字符数组中的日期与数值", "completion",
            "输入形如 \"2026-11-18\" 的日期字符串，使用 sscanf 提取年月日并格式化输出 \"18/11/2026\"。",
            "2026-11-05", "05/11/2026",
            R"JSON([{"label":"日期重排","input":"2026-11-05","expected":"05/11/2026"},{"label":"元旦","input":"2026-01-01","expected":"01/01/2026"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    char s[64];
    scanf("%63s", s);
    int y = 0, m = 0, d = 0;
    // TODO: 用 sscanf 解析 y-m-d 后按 %02d/%02d/%04d 输出
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    char s[64];
    scanf("%63s", s);
    int y = 0, m = 0, d = 0;
    sscanf(s, "%d-%d-%d", &y, &m, &d);
    printf("%02d/%02d/%04d\n", d, m, y);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.6"; l.chapter_id = "ch2"; l.sort_order = 7;
        l.title = "2.6 函数定义、数组作参数与递归调用机制";
        l.source_ref = "《算法笔记》2.6.1-2.6.5"; l.time_comp = "O(2^n) / O(n)"; l.space_comp = "O(n) 栈帧";
        l.viz_type = "stack"; l.viz_label = "函数调用系统栈压栈/出栈与递归边界";
        l.tutorial_html = make_tut(
            "<strong>数组作函数参数传递的是首地址！</strong>因此在函数内修改 <code>void modify(int a[])</code> 的元素会直接改变外部原数组（而普通值传递 <code>int x</code> 只是拷贝）；另外<strong>递归必须包含明确递归边界与缩小规模的递归式</strong>。",
            {
                {"值传递 vs 地址传递", "普通变量默认按值拷贝，形参改变不影响实参；数组传参会退化为首元素指针（二维数组传参第二维大小不可省略，如 <code>int g[][100]</code>）。"},
                {"递归两要素", "① <strong>递归边界</strong>（不再向下调用的出口，防止无限递归 Stack Overflow）；② <strong>递归式</strong>（把大问题分解成同类更小问题）。"}
            }, "",
            "朴素递归算斐波那契 <code>f(n-1) + f(n-2)</code> 会产生海量重复计算达到 <code>O(2^n)</code>，<code>n = 45</code> 就超时；加记忆化或循环递推立刻变为 <code>O(n)</code>。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

long long fact(int n) {
    if (n <= 1) return 1;      // 递归边界
    return n * fact(n - 1);    // 递归式
}

int main() {
    int n; scanf("%d", &n);
    printf("%lld\n", fact(n));
    return 0;
})CPP";
        add_ex(l, "ex_2_6", "练习：递归求解阶乘 / 记忆化斐波那契", "completion",
            "输入整数 n (0 <= n <= 45)，编写记忆化递归（或递推）函数求斐波那契数列第 n 项（F(0)=0, F(1)=1, F(n)=F(n-1)+F(n-2)）。",
            "10", "55",
            R"JSON([{"label":"F(10)=55","input":"10","expected":"55"},{"label":"F(0)=0","input":"0","expected":"0"},{"label":"F(40)","input":"40","expected":"102334155"}])JSON",
            R"CPP(#include <stdio.h>
long long memo[50];
long long fib(int n) {
    // TODO: 补全边界与记忆化递归避免超时
    return 0;
}
int main() {
    int n; scanf("%d", &n);
    printf("%lld\n", fib(n));
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
long long memo[50];
long long fib(int n) {
    if (n <= 1) return n;
    if (memo[n] != 0) return memo[n];
    return memo[n] = fib(n - 1) + fib(n - 2);
}
int main() {
    int n; scanf("%d", &n);
    printf("%lld\n", fib(n));
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.7"; l.chapter_id = "ch2"; l.sort_order = 8;
        l.title = "2.7 从 C 指针 (*p / &a) 进阶到 C++ 引用 (int &a)";
        l.source_ref = "《算法笔记》2.7.1-2.7.5（C → C++ 关键转折点）"; l.time_comp = "O(1)"; l.space_comp = "O(1)";
        l.viz_type = "linked_list"; l.viz_label = "指针存变量内存地址 vs C++ 引用直接起别名";
        l.tutorial_html = make_tut(
            "<strong>本书最重要的 C 到 C++ 语法桥梁！</strong>指针就是<strong>内存地址</strong>：<code>&amp;x</code> 取变量地址，<code>int* p = &amp;x</code> 定义指针，<code>*p</code> 解引用读写目标内存；而在 <strong>C++</strong> 中引入了 <strong>引用 <code>int&amp; r</code></strong>（给原变量直接起别名），传参修改外部变量再也不用写满屏星号了！",
            {
                {"C 语言指针版 swap", "<code>void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }</code> —— 调用时必须传地址 <code>swap(&amp;x, &amp;y)</code>。"},
                {"指针与数组等价关系", "数组名 <code>a</code> 就是首元素地址 <code>&amp;a[0]</code>，<code>*(a + i)</code> 完全等价于 <code>a[i]</code>！"},
                {"C++ 引用版 swap（更安全优雅）", "<code>void swap(int &amp;a, int &amp;b)</code> —— 调用只需 <code>swap(x, y)</code>，且引用的变量永远非空！第 7/9 章链表树插删必用 <code>Node*&amp; root</code>（指针的引用）。"}
            }, "",
            "绝对不要返回函数内局部变量的指针或引用！函数一旦返回，局部栈变量空间立刻回收，变成危险的野指针！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

void swapPtr(int* a, int* b) { int t = *a; *a = *b; *b = t; }
void swapRef(int& a, int& b) { int t = a; a = b; b = t; } // C++ 引用进阶

int main() {
    int x = 3, y = 8;
    swapPtr(&x, &y);
    swapRef(x, y);
    printf("%d %d\n", x, y);
    return 0;
})CPP";
        add_ex(l, "ex_2_7", "练习：指针交换 (C) & 引用倍增 (C++)", "completion",
            "输入两个整数 x, y，补全基于指针的 swapPtr 和基于 C++ 引用的 doubleBoth(&x, &y)，先交换再同时乘以 2 后输出。",
            "3 5", "10 6",
            R"JSON([{"label":"指针+引用组合","input":"3 5","expected":"10 6"},{"label":"含零负数","input":"-4 0","expected":"0 -8"}])JSON",
            R"CPP(#include <stdio.h>
void swapPtr(int* a, int* b) {
    // TODO: C 指针交换 *a 和 *b
}
void doubleRef(int& a, int& b) {
    // TODO: C++ 引用把两个原变量都乘以 2
}
int main() {
    int x, y;
    scanf("%d %d", &x, &y);
    swapPtr(&x, &y);
    doubleRef(x, y);
    printf("%d %d\n", x, y);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
void swapPtr(int* a, int* b) {
    int t = *a; *a = *b; *b = t;
}
void doubleRef(int& a, int& b) {
    a *= 2; b *= 2;
}
int main() {
    int x, y;
    scanf("%d %d", &x, &y);
    swapPtr(&x, &y);
    doubleRef(x, y);
    printf("%d %d\n", x, y);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.8"; l.chapter_id = "ch2"; l.sort_order = 9;
        l.title = "2.8 结构体 (struct) 定义、-> 指针访问与 C++ 构造函数";
        l.source_ref = "《算法笔记》2.8.1-2.8.3"; l.time_comp = "O(n log n)"; l.space_comp = "O(n)";
        l.viz_type = "two_ptr"; l.viz_label = "struct 聚合多字段：实体用 '.'，指针用 '->'";
        l.tutorial_html = make_tut(
            "<strong>结构体把相关属性打包成自定义类型：</strong>普通结构体变量用点号 <code>stu.score</code> 访问；而结构体指针（链表/二叉树结点 <code>Node* p</code>）用箭头 <code>p-&gt;val</code>（等价于 <code>(*p).val</code>）！",
            {
                {"C vs C++ 结构体初始化", "C 里逐字段赋值或初始化列表；<strong>C++ 允许在 struct 内写构造函数</strong>，如 <code>Node(int _x, Node* _n = nullptr) : x(_x), next(_n) {}</code>，让 <code>new Node(5)</code> 一行完成创建！"},
                {"结构体内不能包含自身实例", "不能在 <code>struct Node</code> 里直接放 <code>Node next;</code>（无限大），只能放<strong>指向自身类型的指针 <code>Node* next;</code></strong>！"}
            }, "",
            "C++ 中自定义结构体构造函数之后，如果没写无参默认构造 <code>Node() {}</code>，直接开数组 <code>Node arr[100];</code> 会报编译错误，务必多加一句 <code>Node() = default;</code> 或带默认值参数！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

struct Point {
    int x, y;
    Point() = default;
    Point(int _x, int _y) : x(_x), y(_y) {} // C++ 构造函数
};

int main() {
    Point p(3, 4);
    Point* ptr = &p;
    printf("dot=%d arrow=%d\n", p.x, ptr->y);
    return 0;
})CPP";
        add_ex(l, "ex_2_8", "练习：结构体距离平方与指针访问", "completion",
            "读入点 A(x1,y1) 和 点 B(x2,y2) 封装到 Point 结构体，编写 distSq(const Point* a, const Point* b) 使用 -> 计算两点距离的平方。",
            "1 2 4 6", "25",
            R"JSON([{"label":"3-4-5 直角","input":"1 2 4 6","expected":"25"},{"label":"同一点","input":"5 5 5 5","expected":"0"}])JSON",
            R"CPP(#include <stdio.h>
struct Point { int x, y; };
long long distSq(const Point* a, const Point* b) {
    // TODO: 使用 a->x 等计算 (dx*dx + dy*dy)
    return 0;
}
int main() {
    Point p1, p2;
    scanf("%d %d %d %d", &p1.x, &p1.y, &p2.x, &p2.y);
    printf("%lld\n", distSq(&p1, &p2));
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
struct Point { int x, y; };
long long distSq(const Point* a, const Point* b) {
    long long dx = a->x - b->x, dy = a->y - b->y;
    return dx * dx + dy * dy;
}
int main() {
    Point p1, p2;
    scanf("%d %d %d %d", &p1.x, &p1.y, &p2.x, &p2.y);
    printf("%lld\n", distSq(&p1, &p2));
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.9"; l.chapter_id = "ch2"; l.sort_order = 10;
        l.title = "2.9 C 到 C++ 补充：cin/cout、浮点数 eps 比较与圆周率 Pi";
        l.source_ref = "《算法笔记》2.9.1-2.9.3"; l.time_comp = "O(1)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "浮点数在数轴上允许 ±eps=1e-8 微小误差范围";
        l.tutorial_html = make_tut(
            "<strong>本书正式给出 <code>cin/cout</code> 规范及全书通用的高精度浮点判等宏：</strong>因计算机无法精确存下所有小数，判断 <code>a == b</code> 必须用 <code>fabs(a - b) &lt; eps</code>！",
            {
                {"四大浮点比较宏（《算法笔记》原书必背）", "<code>#define Equ(a,b) (fabs((a)-(b)) &lt; eps)</code>、<code>More(a,b) ((a)-(b) &gt; eps)</code>、<code>Less(a,b) ((a)-(b) &lt; -eps)</code>。"},
                {"精确圆周率常量", "不用手记 3.14159，直接写 <code>const double Pi = acos(-1.0);</code> 最精确。"},
                {"cin/cout 控小数位数", "配合 <code>#include &lt;iomanip&gt;</code>：<code>cout &lt;&lt; fixed &lt;&lt; setprecision(2) &lt;&lt; x;</code>（或者直接 C 风格 <code>printf(\"%.2f\", x)</code> 更简短！）。"}
            }, "",
            "<code>fabs(-0.0)</code> 输出可能出现 <code>-0.00</code>，输出前判断若 <code>fabs(x) &lt; 1e-8</code> 则强制置为 <code>0.0</code>。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <iomanip>
#include <cmath>
using namespace std;

const double EPS = 1e-8;
const double PI = acos(-1.0);

int main() {
    double a, b; cin >> a >> b;
    cout << (fabs(a - b) < EPS ? "EQUAL" : "DIFF") << "\n";
    cout << fixed << setprecision(4) << PI << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_2_9", "练习：浮点误差判等 (eps=1e-8)", "completion",
            "输入两个双精度浮点数 a, b，判断是否满足 |a - b| < 1e-8，相等输出 YES，否则输出 NO。",
            "0.300000001 0.300000002", "YES",
            R"JSON([{"label":"微小误差相等","input":"0.300000001 0.300000002","expected":"YES"},{"label":"真实不等","input":"0.3 0.31","expected":"NO"}])JSON",
            R"CPP(#include <iostream>
#include <cmath>
using namespace std;
const double EPS = 1e-8;
int main() {
    double a, b; cin >> a >> b;
    // TODO: 用 EPS 判断 a 与 b 是否相等
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <cmath>
using namespace std;
const double EPS = 1e-8;
int main() {
    double a, b; cin >> a >> b;
    cout << (fabs(a - b) < EPS ? "YES" : "NO") << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "2.10"; l.chapter_id = "ch2"; l.sort_order = 11;
        l.title = "2.10 黑盒测试三类输入模型 (EOF / 给定 T / 哨兵 0 结束)";
        l.source_ref = "《算法笔记》2.10.1-2.10.2"; l.time_comp = "O(T · n)"; l.space_comp = "O(1)";
        l.viz_type = "two_ptr"; l.viz_label = "scanf != EOF 读到输入流结束";
        l.tutorial_html = make_tut(
            "<strong>上机实战第一道关卡就是「怎么知道输入什么时候结束？」：</strong>原书总结了黑盒测试全覆盖三种格式。",
            {
                {"模型 1：EOF 一直读到尾", "C 写法：<code>while (scanf(\"%d %d\", &amp;a, &amp;b) != EOF)</code>；C++ 写法：<code>while (cin &gt;&gt; a &gt;&gt; b)</code>。"},
                {"模型 2：约定哨兵值退出", "如读到 <code>a == 0 &amp;&amp; b == 0</code> 停止：<code>while (scanf(\"%d%d\", &amp;a, &amp;b) != EOF &amp;&amp; (a || b))</code>（或循环内部 <code>if (!a &amp;&amp; !b) break;</code>）。"},
                {"模型 3：第一行先给组数 T", "<code>int T; scanf(\"%d\", &amp;T); while (T--) { ... }</code> —— 注意每组内部一定要<strong>重置累计变量或清空状态数组</strong>！"}
            }, "",
            "多组测试中最惨的 WA 是上一组留下的脏数据污染了下一组！每组开头务必重新初始化 <code>sum = 0;</code> 或 <code>memset(...)</code>。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    int a, b;
    while (scanf("%d %d", &a, &b) != EOF) {
        if (a == 0 && b == 0) break; // 哨兵值退出
        printf("%d\n", a + b);
    }
    return 0;
})CPP";
        add_ex(l, "ex_2_10", "练习：哨兵 (0 0) 终止的多点测试求和", "completion",
            "读取多行成对整数 (a, b)，遇到 0 0 时结束程序（不输出 0 0 的和），其余每行输出 a + b。",
            "1 5\n10 20\n0 0", "6\n30",
            R"JSON([{"label":"哨兵终止","input":"1 5\n10 20\n0 0","expected":"6\n30"},{"label":"仅 EOF","input":"3 4","expected":"7"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    int a, b;
    // TODO: 读到 EOF 或 (0, 0) 时退出，否则输出 a + b
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    int a, b;
    while (scanf("%d %d", &a, &b) == 2) {
        if (a == 0 && b == 0) break;
        printf("%d\n", a + b);
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 3 章｜入门篇（1）——入门模拟（《算法笔记》Ch3 全 6 节）
    // =========================================================================
    chapters.push_back({"ch3", 3, "C语言入门(Ch0-3)", "第 3 章｜入门模拟全 6 节（3n+1 / 查找 / 图形 / 日期 / 进制 / 串）", "《算法笔记》Ch3 · 3.1-3.6 原书全节"});

    {
        DsLesson l; l.id = "3.1"; l.chapter_id = "ch3"; l.sort_order = 1;
        l.title = "3.1 简单模拟（Callatz 3n+1 猜想 & P1328 查表）";
        l.source_ref = "《算法笔记》3.1 · 前导 P1328"; l.time_comp = "O(steps)"; l.space_comp = "O(1)";
        l.viz_type = "complexity"; l.viz_label = "奇偶分支模拟直到收敛到 1";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》开篇经典 PAT B1001 害死人不偿命的 (3n+1) 猜想：</strong>偶数砍半 <code>n /= 2</code>，奇数令 <code>n = (3*n + 1) / 2</code>，统计变到 <code>1</code> 需要几步。",
            {
                {"严谨读清题意", "B1001 要求奇数是将 <code>3n + 1</code> <strong>砍掉一半</strong>计 1 步（而非 2 步），写错直接全挂。"},
                {"多规则模拟查表化", "如石头剪刀布 P1328 或挖掘机学校得分 <code>school[id] += score</code>，用数组直接按编号累加最清晰。"}
            }, "",
            "做题时注意每一步中间结果会不会超过 <code>int</code>（比如标准 3n+1 在输入较大时需使用 <code>long long</code>）。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    int n, step = 0;
    scanf("%d", &n);
    while (n != 1) {
        if (n % 2 == 0) n /= 2;
        else n = (3 * n + 1) / 2;
        step++;
    }
    printf("%d\n", step);
    return 0;
})CPP";
        add_ex(l, "ex_3_1", "练习：Callatz 3n+1 步数统计 (PAT B1001)", "completion",
            "输入正整数 n (<=1000)，偶数减半、奇数取 (3n+1)/2，输出多少步变为 1。",
            "3", "5",
            R"JSON([{"label":"PAT B1001 样例","input":"3","expected":"5"},{"label":"边界 n=1","input":"1","expected":"0"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    int n, step = 0;
    scanf("%d", &n);
    // TODO: 按规则模拟直到 n == 1
    printf("%d\n", step);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    int n, step = 0;
    scanf("%d", &n);
    while (n != 1) {
        if (n % 2 == 0) n /= 2;
        else n = (3 * n + 1) / 2;
        step++;
    }
    printf("%d\n", step);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "3.2"; l.chapter_id = "ch3"; l.sort_order = 2;
        l.title = "3.2 查找元素（数组顺序查找与桶统计）";
        l.source_ref = "《算法笔记》3.2"; l.time_comp = "O(n)"; l.space_comp = "O(n)";
        l.viz_type = "bsearch"; l.viz_label = "无序序列线性扫描匹配首个目标下标";
        l.tutorial_html = make_tut(
            "<strong>在序列尚未排序时，顺序扫描下标是最直接的做法：</strong>若数据规模小（<code>n ≤ 1000</code>）只需 <code>for</code> 一重循环匹配 <code>a[i] == x</code>，找不到输出 <code>-1</code>。",
            {
                {"无序查找与哈希桶查找对比", "如果每次查询范围在 <code>0..10^5</code> 且多组查询，开一个 <code>int pos[100005]</code> 直接存值对应下标，能做到 <code>O(1)</code> 查询！"}
            }, "",
            "下标从 <code>0</code> 开始还是从 <code>1</code> 开始输出，读题务必确认！"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int a[205];
int main() {
    int n, x, idx = -1;
    scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    scanf("%d", &x);
    for (int i = 0; i < n; ++i) {
        if (a[i] == x) { idx = i; break; }
    }
    printf("%d\n", idx);
    return 0;
})CPP";
        add_ex(l, "ex_3_2", "练习：顺序查找目标值下标", "completion",
            "输入 n 个整数后输入目标 x，输出 x 第一次出现的 0-based 下标；找不到输出 -1。",
            "4\n10 20 30 20\n20", "1",
            R"JSON([{"label":"首个命中","input":"4\n10 20 30 20\n20","expected":"1"},{"label":"不存在","input":"3\n1 2 3\n9","expected":"-1"}])JSON",
            R"CPP(#include <stdio.h>
int a[1005];
int main() {
    int n, x, idx = -1;
    scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    scanf("%d", &x);
    // TODO: 顺序查找首次等于 x 的下标
    printf("%d\n", idx);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int a[1005];
int main() {
    int n, x, idx = -1;
    scanf("%d", &n);
    for (int i = 0; i < n; ++i) scanf("%d", &a[i]);
    scanf("%d", &x);
    for (int i = 0; i < n; ++i) {
        if (a[i] == x) { idx = i; break; }
    }
    printf("%d\n", idx);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "3.3"; l.chapter_id = "ch3"; l.sort_order = 3;
        l.title = "3.3 图形输出（正方形/梯形字符画排版）";
        l.source_ref = "《算法笔记》3.3"; l.time_comp = "O(r·c)"; l.space_comp = "O(1)";
        l.viz_type = "two_ptr"; l.viz_label = "逐行计算前置空格数与每行字符数规律";
        l.tutorial_html = make_tut(
            "<strong>图形输出题（PAT 跟奥巴马一起画正方形 / 沙漏）只需找出「第 i 行有几个空格、几个符号」：</strong>切忌在行末输出多余空格，右侧只需换行即可！",
            {
                {"双重循环直接输出法", "先算总行数 <code>row = (n + 1) / 2</code>（四舍五入取半），首尾行全部打字符 <code>ch</code>，中间行只打首尾两个 <code>ch</code> 中间打空格。"},
                {"二维字符画布法（复杂图形推荐）", "如斜菱形/螺旋沙漏，先开 <code>char mp[105][105]</code> 全填空格，把符号摆进坐标再逐行打印。"}
            }, "",
            "题目要求行数为列数 50% 四舍五入时，用 <code>int row = (col + 1) / 2;</code> 或 <code>round(col / 2.0)</code>。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    int n; char ch;
    scanf("%d %c", &n, &ch);
    int row = (n + 1) / 2;
    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == 0 || i == row - 1 || j == 0 || j == n - 1) printf("%c", ch);
            else printf(" ");
        }
        printf("\n");
    }
    return 0;
})CPP";
        add_ex(l, "ex_3_3", "练习：空心正方形字符输出 (PAT B1036)", "completion",
            "输入边长 N 和字符 C，输出 N 列、行数为 (N+1)/2 的空心矩形边界。",
            "5 *", "*****\n*   *\n*****",
            R"JSON([{"label":"5 列矩形","input":"5 *","expected":"*****\n*   *\n*****"},{"label":"4 列矩形","input":"4 #","expected":"####\n####"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    int n; char ch;
    scanf("%d %c", &n, &ch);
    int row = (n + 1) / 2;
    // TODO: 输出 row 行 n 列的空心字符边框
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    int n; char ch;
    scanf("%d %c", &n, &ch);
    int row = (n + 1) / 2;
    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == 0 || i == row - 1 || j == 0 || j == n - 1) putchar(ch);
            else putchar(' ');
        }
        putchar('\n');
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "3.4"; l.chapter_id = "ch3"; l.sort_order = 4;
        l.title = "3.4 日期处理（平闰年月份表与两天相差天数）";
        l.source_ref = "《算法笔记》3.4"; l.time_comp = "O(Δdays)"; l.space_comp = "O(1)";
        l.viz_type = "two_ptr"; l.viz_label = "小日期逐日加 1 处理跨月跨年直到追上大日期";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》最强防错日期模板：「永远不要去凑跨年/跨月加减法，让较小的日期每次加 1 天并处理进位，直到追上大日期」！</strong>哪怕相隔 100 年才循环 36500 次（0.1 毫秒），彻底消灭闰年与大小月边界 Bug！",
            {
                {"二维月份天数表", "<code>int mon[13][2] = {{0,0},{31,31},{28,29},{31,31},...};</code> 第二维 <code>0</code> 存平年，<code>1</code> 存闰年！"},
                {"加 1 天进位三行代码", "<code>d++; if (d == mon[m][isLeap(y)] + 1) { m++; d = 1; } if (m == 13) { y++; m = 1; } ans++;</code>"}
            }, "",
            "若输入两个日期不确定先后，开局用 <code>if (t1 > t2) swap(t1, t2);</code> 保证第一个永远不晚于第二个；另看清题目问的是「相差天数」还是「包含首尾共有几天（需 +1）」。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int mon[13][2] = {{0,0},{31,31},{28,29},{31,31},{30,30},{31,31},{30,30},{31,31},{31,31},{30,30},{31,31},{30,30},{31,31}};
int isLeap(int y) { return (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0); }

int main() {
    int d1, d2; scanf("%d %d", &d1, &d2);
    if (d1 > d2) { int t = d1; d1 = d2; d2 = t; }
    int y1 = d1 / 10000, m1 = d1 % 10000 / 100, day1 = d1 % 100;
    int y2 = d2 / 10000, m2 = d2 % 10000 / 100, day2 = d2 % 100;
    int ans = 1;
    while (y1 < y2 || m1 < m2 || day1 < day2) {
        day1++;
        if (day1 == mon[m1][isLeap(y1)] + 1) { m1++; day1 = 1; }
        if (m1 == 13) { y1++; m1 = 1; }
        ans++;
    }
    printf("%d\n", ans);
    return 0;
})CPP";
        add_ex(l, "ex_3_4", "练习：两日期之间间隔天数 (含首尾)", "completion",
            "输入两个 8 位整数格式日期 YYYYMMDD（如 20240228 和 20240301），输出包括首尾一共相隔多少天。",
            "20240228 20240301", "3",
            R"JSON([{"label":"闰年跨2月29","input":"20240228 20240301","expected":"3"},{"label":"平年无29日","input":"20230228 20230301","expected":"2"},{"label":"同一天","input":"20260501 20260501","expected":"1"}])JSON",
            R"CPP(#include <stdio.h>
int mon[13][2] = {{0,0},{31,31},{28,29},{31,31},{30,30},{31,31},{30,30},{31,31},{31,31},{30,30},{31,31},{30,30},{31,31}};
int isLeap(int y) { return (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0); }
int main() {
    int a, b; scanf("%d %d", &a, &b);
    if (a > b) { int t = a; a = b; b = t; }
    int y1 = a / 10000, m1 = a % 10000 / 100, d1 = a % 100;
    int y2 = b / 10000, m2 = b % 10000 / 100, d2 = b % 100;
    int ans = 1;
    // TODO: 逐天递增 y1-m1-d1 直到等于 y2-m2-d2
    printf("%d\n", ans);
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int mon[13][2] = {{0,0},{31,31},{28,29},{31,31},{30,30},{31,31},{30,30},{31,31},{31,31},{30,30},{31,31},{30,30},{31,31}};
int isLeap(int y) { return (y % 400 == 0) || (y % 4 == 0 && y % 100 != 0); }
int main() {
    int a, b; scanf("%d %d", &a, &b);
    if (a > b) { int t = a; a = b; b = t; }
    int y1 = a / 10000, m1 = a % 10000 / 100, d1 = a % 100;
    int y2 = b / 10000, m2 = b % 10000 / 100, d2 = b % 100;
    int ans = 1;
    while (y1 < y2 || m1 < m2 || d1 < d2) {
        d1++;
        if (d1 == mon[m1][isLeap(y1)] + 1) { m1++; d1 = 1; }
        if (m1 == 13) { y1++; m1 = 1; }
        ans++;
    }
    printf("%d\n", ans);
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "3.5"; l.chapter_id = "ch3"; l.sort_order = 5;
        l.title = "3.5 进制转换（P 进制 ↔ 十进制 ↔ Q 进制）";
        l.source_ref = "《算法笔记》3.5"; l.time_comp = "O(log n)"; l.space_comp = "O(log n)";
        l.viz_type = "stack"; l.viz_label = "除基取余倒序入数组，再逆序输出高位到低位";
        l.tutorial_html = make_tut(
            "<strong>任意 <code>P</code> 进制与 <code>Q</code> 进制互转，统一以十进制做桥梁：</strong>① <code>P</code> 进制转十进制用<strong>秦九韶迭代乘基加位</strong>；② 十进制转 <code>Q</code> 进制用 <strong><code>do { ans[num++] = y % Q; y /= Q; } while (y != 0);</code> 除基倒取余</strong>！",
            {
                {"P → 十进制 C 循环", "<code>int y = 0, prod = 1; while (x) { y += (x % 10) * prod; x /= 10; prod *= P; }</code>（或从字符串高位遍历 <code>y = y * P + digit</code>）。"},
                {"十进制 → Q 进制", "用 <code>do-while</code> 保证数字恰好为 <code>0</code> 时数组内也存入 1 个 <code>0</code>，最后从 <code>num - 1</code> 倒序打印到 <code>0</code>。"}
            }, "",
            "若输入超过 <code>int</code> 范围记得用 <code>long long</code>；十六进制记得把 10–15 映射成 <code>'A' + val - 10</code>。"
        );
        l.code_template =
R"CPP(#include <stdio.h>

int main() {
    long long a, b; int d;
    scanf("%lld %lld %d", &a, &b, &d);
    long long sum = a + b;
    int ans[64], num = 0;
    do { ans[num++] = (int)(sum % d); sum /= d; } while (sum != 0);
    for (int i = num - 1; i >= 0; --i) printf("%d", ans[i]);
    printf("\n");
    return 0;
})CPP";
        add_ex(l, "ex_3_5", "练习：D 进制的 A+B (PAT B1022)", "completion",
            "输入两个非负整数 A, B 和进制 D (2 <= D <= 16)，输出 A + B 的 D 进制表示（字母大写）。",
            "123 456 8", "1103",
            R"JSON([{"label":"PAT B1022 样例","input":"123 456 8","expected":"1103"},{"label":"零边界","input":"0 0 10","expected":"0"},{"label":"16进制","input":"100 155 16","expected":"FF"}])JSON",
            R"CPP(#include <stdio.h>
int main() {
    long long a, b; int d;
    scanf("%lld %lld %d", &a, &b, &d);
    long long sum = a + b;
    char digits[] = "0123456789ABCDEF", buf[64];
    int len = 0;
    // TODO: do-while 除基取余存入 buf 并逆序输出
    printf("\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
int main() {
    long long a, b; int d;
    scanf("%lld %lld %d", &a, &b, &d);
    long long sum = a + b;
    char digits[] = "0123456789ABCDEF", buf[64];
    int len = 0;
    do {
        buf[len++] = digits[sum % d];
        sum /= d;
    } while (sum > 0);
    for (int i = len - 1; i >= 0; --i) putchar(buf[i]);
    putchar('\n');
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "3.6"; l.chapter_id = "ch3"; l.sort_order = 6;
        l.title = "3.6 字符串处理（C 字符数组判回文与说反话单词拆分）";
        l.source_ref = "《算法笔记》3.6"; l.time_comp = "O(L)"; l.space_comp = "O(L)";
        l.viz_type = "two_ptr"; l.viz_label = "首尾双指针向中间收拢检查对称性";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》高频送分专题：字符串处理（PAT B1009 说反话 & 判回文）！</strong>读取含空格整行可用逐字符 <code>while (scanf(\"%s\", word[num]) != EOF) num++;</code> 或二维字符数组拆词，比手撕空格指针省力十倍！",
            {
                {"回文串首尾比对", "设串长 <code>len = strlen(s)</code>，枚举 <code>i = 0 .. len/2 - 1</code> 检查 <code>if (s[i] != s[len - 1 - i]) return false;</code>。"},
                {"英文句子单词倒序（说反话）", "利用 <code>scanf(\"%s\", words[cnt])</code> 遇到空格自动分词的特性直接收入二维字符数组，再倒序输出！"}
            }, "",
            "若题目只给一行且用 <code>fgets</code> 读入，注意行尾会包含多余的 <code>'\\n'</code> 或 <code>'\\r'</code>，需要手动替换成 <code>'\\0'</code> 再做回文判断！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <string.h>

int main() {
    char s[260];
    scanf("%255s", s);
    int n = (int)strlen(s), ok = 1;
    for (int i = 0; i < n / 2; ++i) {
        if (s[i] != s[n - 1 - i]) { ok = 0; break; }
    }
    printf("%s\n", ok ? "YES" : "NO");
    return 0;
})CPP";
        add_ex(l, "ex_3_6", "练习：整句英文单词说反话 (PAT B1009)", "completion",
            "读入一行由空格分隔的若干英语单词（不含标点），将所有单词按逆序输出（行末无多余空格）。",
            "Hello World Here I Come", "Come I Here World Hello",
            R"JSON([{"label":"PAT B1009 说反话","input":"Hello World Here I Come","expected":"Come I Here World Hello"},{"label":"单词唯一","input":"Algo","expected":"Algo"}])JSON",
            R"CPP(#include <stdio.h>
char words[100][85];
int main() {
    int cnt = 0;
    // TODO: 循环 scanf("%s", words[cnt]) 到 EOF 后倒序打印单词
    printf("\n");
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
char words[100][85];
int main() {
    int cnt = 0;
    while (scanf("%84s", words[cnt]) != EOF) cnt++;
    for (int i = cnt - 1; i >= 0; --i) {
        if (i < cnt - 1) printf(" ");
        printf("%s", words[i]);
    }
    printf("\n");
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }
}

} // namespace ds_seed
} // namespace omni
