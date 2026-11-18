#pragma once

#include "ds_seed_common.hpp"

namespace omni {
namespace ds_seed {

inline void seed_ch11_ch13(std::vector<DsChapter>& chapters, std::vector<DsLesson>& lessons) {
    // =========================================================================
    // 第 11 章｜提高篇（5）——动态规划专题（《算法笔记》Ch11 全 8 节）
    // =========================================================================
    chapters.push_back({"ch11", 11, "图论/DP/高阶(Ch10-13)", "第 11 章｜动态规划全专题：线性/子序列/DAG/背包与区间DP（11.1-11.8）", "《算法笔记》Ch11 · 11.1-11.8 原书全 8 节"});

    {
        DsLesson l; l.id = "11.1"; l.chapter_id = "ch11"; l.sort_order = 1;
        l.title = "11.1 动态规划的递归写法与递推写法（数塔问题）";
        l.source_ref = "《算法笔记》11.1"; l.time_comp = "O(n^2)"; l.space_comp = "O(n^2)";
        l.viz_type = "dp"; l.viz_label = "记忆化搜索（自顶向下）vs 递推填表（自底向上）";
        l.tutorial_html = make_tut(
            "<strong>什么是动态规划（DP）？《算法笔记》用“数塔问题”点破核心：</strong>若一个问题的最优解包含其子问题的最优解（<strong>最优子结构</strong>），且暴力递归时会反复算同一子问题（<strong>重叠子问题</strong>），只需开一张表 <code>dp[][]</code> 记住答案即可从指数级降至多项式级！",
            {
                {"两种等价实现方式", "① <strong>自顶向下（记忆化搜索）</strong>：递归前先查 <code>if (dp[i][j] != -1) return dp[i][j];</code>；<br>② <strong>自底向上（递推，推荐）</strong>：从最底层叶子出发，<code>for (int i = n - 1; i &gt;= 1; --i) dp[i][j] = max(dp[i+1][j], dp[i+1][j+1]) + f[i][j];</code>，最终答案就是 <code>dp[1][1]</code>！"}
            }, "",
            "递推 DP 必须保证计算当前状态时，它依赖的所有前置子状态已经算完（即满足拓扑无后效性顺序）！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    vector<vector<int>> tri = {
        {5},
        {8, 3},
        {12, 7, 16},
        {4, 10, 11, 6},
        {9, 5, 3, 9, 4}
    };
    int n = tri.size();
    vector<vector<int>> dp = tri;
    for (int i = n - 2; i >= 0; --i) {
        for (int j = 0; j <= i; ++j) {
            dp[i][j] += max(dp[i + 1][j], dp[i + 1][j + 1]);
        }
    }
    cout << "max path sum = " << dp[0][0] << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_1", "练习：数塔问题（数字三角形从顶到底最大路径和）", "completion",
            "输入数字三角形层数 n，接下来第 i 行给出 i 个整数（1 <= i <= n）。每步只能走到下一行相邻的左下或右下数字，求从顶到底的最大路径数字和。",
            "5\n5\n8 3\n12 7 16\n4 10 11 6\n9 5 3 9 4", "44",
            R"JSON([{"label":"《算法笔记》原书5层数塔","input":"5\n5\n8 3\n12 7 16\n4 10 11 6\n9 5 3 9 4","expected":"44"},{"label":"单层数塔","input":"1\n-7","expected":"-7"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<vector<int>> dp(n);
    for (int i = 0; i < n; ++i) {
        dp[i].resize(i + 1);
        for (int j = 0; j <= i; ++j) cin >> dp[i][j];
    }
    // TODO: 自底向上递推填充 dp，输出顶部 dp[0][0]
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<vector<int>> dp(n);
    for (int i = 0; i < n; ++i) {
        dp[i].resize(i + 1);
        for (int j = 0; j <= i; ++j) cin >> dp[i][j];
    }
    for (int i = n - 2; i >= 0; --i) {
        for (int j = 0; j <= i; ++j) {
            dp[i][j] += max(dp[i + 1][j], dp[i + 1][j + 1]);
        }
    }
    cout << dp[0][0] << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "11.2"; l.chapter_id = "ch11"; l.sort_order = 2;
        l.title = "11.2 最大连续子序列和 (Kadane 状态定义：以 i 结尾)";
        l.source_ref = "《算法笔记》11.2"; l.time_comp = "O(n)"; l.space_comp = "O(n) / O(1)";
        l.viz_type = "dp"; l.viz_label = "线性 DP 经典思想：dp[i] 表示“必须以 a[i] 结尾”的最大和";
        l.tutorial_html = make_tut(
            "<strong>线性序列 DP 最关键的一步设计技巧：</strong>如果直接设 <code>dp[i]</code> 为前 <code>i</code> 个数的最优解，往往难以判断能否接着拼在后面；强行定义 <strong><code>dp[i]</code> 为“必须以 <code>a[i]</code> 作为右端点结尾”的最大连续子序列和</strong>，转移瞬间水落石出！",
            {
                {"状态转移方程（仅 1 行）", "以 <code>a[i]</code> 结尾的连续段要么只有 <code>a[i]</code> 自己，要么接在 <code>a[i-1]</code> 结尾的段后面：<br><code>dp[i] = max(a[i], dp[i - 1] + a[i]);</code><br>全局答案则为所有 <code>dp[0..n-1]</code> 的最大值 <code>*max_element(...)</code>！"}
            }, "",
            "全局初始化最大值 <code>ans</code> 绝对不能随便设为 <code>0</code>！万一整个数组全是负数（如 <code>[-5, -2, -9]</code>），正确非空最大子段和应为 <code>-2</code>，设为 0 会误输出 0！正确初值是 <code>ans = dp[0] = a[0]</code>。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    vector<int> a = {-2, 11, -4, 13, -5, -2};
    int cur = a[0], best = a[0];
    for (int i = 1; i < static_cast<int>(a.size()); ++i) {
        cur = max(a[i], cur + a[i]);
        best = max(best, cur);
    }
    cout << "max subarray sum = " << best << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_2", "练习：求解非空最大连续子序列和", "completion",
            "输入 n 与 n 个整数（可能全为负数），输出任意长度至少为 1 的连续子序列最大元素和。",
            "6\n-2 11 -4 13 -5 -2", "20",
            R"JSON([{"label":"《算法笔记》原书样例","input":"6\n-2 11 -4 13 -5 -2","expected":"20"},{"label":"全负数序列","input":"4\n-8 -3 -5 -9","expected":"-3"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<long long> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    // TODO: 递推计算以 a[i] 结尾的最大子段和并维护全局最大值
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<long long> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    long long cur = a[0], best = a[0];
    for (int i = 1; i < n; ++i) {
        cur = max(a[i], cur + a[i]);
        best = max(best, cur);
    }
    cout << best << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "11.3"; l.chapter_id = "ch11"; l.sort_order = 3;
        l.title = "11.3 最长不下降/上升子序列 (LIS) 与 O(n log n) 二分优化";
        l.source_ref = "《算法笔记》11.3"; l.time_comp = "O(n log n)"; l.space_comp = "O(n)";
        l.viz_type = "dp"; l.viz_label = "状态表 dp[i] O(n^2) vs tails[] + lower_bound O(n log n)";
        l.tutorial_html = make_tut(
            "<strong>子序列允许不连续，只要原相对前后顺序不变即可！</strong>",
            {
                {"基础 O(n^2) 写法", "令 <code>dp[i]</code> 表示以 <code>a[i]</code> 结尾的 LIS 长度，初值 1：<br><code>for (int j = 0; j &lt; i; ++j) if (a[j] &lt; a[i]) dp[i] = max(dp[i], dp[j] + 1);</code>"},
                {"进阶 O(n log n) 贪心 + 二分优化", "维护 <code>tails[k]</code> 表示长度为 <code>k+1</code> 的上升子序列的<strong>最小可能末尾元素</strong>（天然严格递增！）：<br>• 严格上升（<code>a[j] &lt; a[i]</code>）：用 <code>lower_bound</code> 替换首个 <code>&gt;= x</code>；<br>• 不下降（<code>a[j] &lt;= a[i]</code>）：用 <code>upper_bound</code> 替换首个 <code>&gt; x</code>！"}
            }, "",
            "区分清楚：<code>tails</code> 数组的长度就是 LIS 的最终长度，但 <code>tails</code> 里存的数字本身不一定是合法子序列元素（因为被后来更小的值贪心替换过）。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    vector<int> a = {10, 9, 2, 5, 3, 7, 101, 18};
    vector<int> tails;
    for (int x : a) {
        auto it = lower_bound(tails.begin(), tails.end(), x);
        if (it == tails.end()) tails.push_back(x);
        else *it = x;
    }
    cout << "LIS len = " << tails.size() << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_3", "练习：O(n log n) 求最长严格上升子序列长度", "completion",
            "输入 n (1 <= n <= 100000) 及 n 个整数，计算最长严格递增子序列（a[i1] < a[i2] < ...）的长度。",
            "8\n10 9 2 5 3 7 101 18", "4",
            R"JSON([{"label":"严格递增样例","input":"8\n10 9 2 5 3 7 101 18","expected":"4"},{"label":"全相等仅长度为1","input":"4\n7 7 7 7","expected":"1"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    // TODO: 使用 tails + lower_bound 实现 O(n log n) LIS
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<int> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    vector<int> tails;
    for (int x : a) {
        auto it = lower_bound(tails.begin(), tails.end(), x);
        if (it == tails.end()) tails.push_back(x);
        else *it = x;
    }
    cout << tails.size() << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "11.4"; l.chapter_id = "ch11"; l.sort_order = 4;
        l.title = "11.4 最长公共子序列 (LCS)：双串二维状态转移";
        l.source_ref = "《算法笔记》11.4"; l.time_comp = "O(nm)"; l.space_comp = "O(nm)";
        l.viz_type = "dp"; l.viz_label = "二维状态表 dp[i][j]：匹配继承斜左上+1，失配继承上/左最大";
        l.tutorial_html = make_tut(
            "<strong>给定字符串 A 和 B，求同时出现在两者中的最长子序列（不必连续）长度：</strong>",
            {
                {"二维状态定义（1-indexed 最干净）", "令 <code>dp[i][j]</code> 为 <code>A</code> 的前 <code>i</code> 个字符与 <code>B</code> 的前 <code>j</code> 个字符的 LCS 长度（<code>0</code> 行与 <code>0</code> 列全为 0）：<br>• 若 <code>A[i-1] == B[j-1]</code>：<code>dp[i][j] = dp[i - 1][j - 1] + 1;</code><br>• 否则：<code>dp[i][j] = max(dp[i - 1][j], dp[i][j - 1]);</code>"}
            }, "",
            "若字符串采用 0-indexed 存储 <code>string A, B</code>，务必写成 <code>A[i - 1] == B[j - 1]</code>，并且 <code>dp</code> 数组开到 <code>(n + 1) x (m + 1)</code> 来自然容纳全 0 的边界第 0 行与第 0 列！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

int main() {
    string a = "sadstory", b = "adminsorry";
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j)
            dp[i][j] = (a[i - 1] == b[j - 1]) ? dp[i - 1][j - 1] + 1
                                              : max(dp[i - 1][j], dp[i][j - 1]);
    cout << "LCS len = " << dp[n][m] << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_4", "练习：求解两字符串的最长公共子序列 (LCS) 长度", "completion",
            "输入两行非空字符串 A 和 B（无空格，长度 <= 1000），输出它们最长公共子序列（不要求连续）的长度。",
            "sadstory\nadminsorry", "6",
            R"JSON([{"label":"《算法笔记》原书例串","input":"sadstory\nadminsorry","expected":"6"},{"label":"无公共字符","input":"abc\nxyz","expected":"0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

int main() {
    string a, b; cin >> a >> b;
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));
    // TODO: 写出 LCS 二重循环状态转移并输出 dp[n][m]
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

int main() {
    string a, b; cin >> a >> b;
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (a[i - 1] == b[j - 1]) dp[i][j] = dp[i - 1][j - 1] + 1;
            else dp[i][j] = max(dp[i - 1][j], dp[i][j - 1]);
        }
    }
    cout << dp[n][m] << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "11.5"; l.chapter_id = "ch11"; l.sort_order = 5;
        l.title = "11.5 最长回文子串：按子串长度 L 递推布尔表 dp[i][j]";
        l.source_ref = "《算法笔记》11.5"; l.time_comp = "O(n^2)"; l.space_comp = "O(n^2)";
        l.viz_type = "dp"; l.viz_label = "短串向长串递推：s[i]==s[j] 且内层 dp[i+1][j-1] 为真";
        l.tutorial_html = make_tut(
            "<strong>最长回文子串与最长公共子序列不同，它要求“连续”！</strong>设 <code>dp[i][j] = true</code> 表示子串 <code>S[i..j]</code> 是回文串，转移只看两端字符 <code>S[i] == S[j]</code> 且去掉头尾后的内部 <code>S[i+1..j-1]</code> 也是回文！",
            {
                {"按长度 L 从小到大填表（极其重要）", "① 先初始化所有长为 1（<code>dp[i][i] = 1</code>）与长为 2（<code>if (S[i]==S[i+1]) dp[i][i+1]=1, ans=2</code>）；<br>② 再外层枚举子串长度 <code>L = 3..n</code>，内层枚举左端点 <code>i = 0..n-L</code>，右端点 <code>j = i + L - 1</code>，若 <code>S[i] == S[j] &amp;&amp; dp[i+1][j-1]</code> 则 <code>dp[i][j] = 1, ans = L</code>！"}
            }, "",
            "绝对不能直接双循环 <code>for (i=0..n) for (j=i..n)</code> 去求 <code>dp[i][j]</code>！因为算 <code>dp[i][j]</code> 时依赖第 <code>i+1</code> 行，顺着枚举 <code>i</code> 时第 <code>i+1</code> 行尚未计算。要么按长度 <code>L</code> 从小到大枚举，要么从 <code>i = n - 1</code> 倒序枚举！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <string>
using namespace std;

int main() {
    string s = "PATZJUJZTACCBCC";
    int n = s.size(), ans = 1;
    vector<vector<char>> dp(n, vector<char>(n, 0));
    for (int i = 0; i < n; ++i) {
        dp[i][i] = 1;
        if (i + 1 < n && s[i] == s[i + 1]) { dp[i][i + 1] = 1; ans = 2; }
    }
    for (int L = 3; L <= n; ++L) {
        for (int i = 0; i + L - 1 < n; ++i) {
            int j = i + L - 1;
            if (s[i] == s[j] && dp[i + 1][j - 1]) { dp[i][j] = 1; ans = L; }
        }
    }
    cout << "max palindrome len = " << ans << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_5", "练习：求字符串的最长回文连续子串长度 (PAT A1040)", "completion",
            "读入一行非空字符串（可能包含空格及标点符号，长度 <= 1000），输出其最长的连续对称回文子串长度。",
            "Is PAT&TAP symmetric?", "11",
            R"JSON([{"label":"PAT A1040 含空格样例","input":"Is PAT&TAP symmetric?","expected":"11"},{"label":"偶数回文串","input":"abba","expected":"4"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
using namespace std;

int main() {
    string s;
    getline(cin, s);
    while (!s.empty() && s.back() == '\r') s.pop_back();
    // TODO: 区间 DP 按长度 L=1..n 统计最长回文子串长度并输出
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
using namespace std;

int main() {
    string s;
    getline(cin, s);
    while (!s.empty() && s.back() == '\r') s.pop_back();
    if (s.empty()) { cout << 0 << "\n"; return 0; }
    int n = s.size(), ans = 1;
    vector<vector<char>> dp(n, vector<char>(n, 0));
    for (int i = 0; i < n; ++i) {
        dp[i][i] = 1;
        if (i + 1 < n && s[i] == s[i + 1]) { dp[i][i + 1] = 1; ans = 2; }
    }
    for (int L = 3; L <= n; ++L) {
        for (int i = 0; i + L - 1 < n; ++i) {
            int j = i + L - 1;
            if (s[i] == s[j] && dp[i + 1][j - 1]) { dp[i][j] = 1; ans = L; }
        }
    }
    cout << ans << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "11.6"; l.chapter_id = "ch11"; l.sort_order = 6;
        l.title = "11.6 DAG 最长路：图上的记忆化搜索与关键路径求解";
        l.source_ref = "《算法笔记》11.6"; l.time_comp = "O(V + E)"; l.space_comp = "O(V + E)";
        l.viz_type = "graph"; l.viz_label = "DAG 上按记忆化或拓扑序求解任意起点最长有向路径";
        l.tutorial_html = make_tut(
            "<strong>在一般有环图里求最长路是 NP 难问题，但一旦是有向无环图（DAG），就能用记忆化搜索或拓扑序在 <code>O(V + E)</code> 线性时间内精准解决！</strong>",
            {
                {"模型一：任意终点最大路径权值", "设 <code>dp[i]</code> 为从顶点 <code>i</code> 出发能走出的最长路径长度：<code>int DP(int u) { if (vis[u]) return dp[u]; vis[u]=1; for (int i=0; i&lt;adj[u].size(); ++i) dp[u] = max(dp[u], DP(adj[u][i].v) + adj[u][i].w); return dp[u]; }</code>（出度为 0 的终点自动返回 0！）。"},
                {"模型二：固定到达终点 T 的最长路", "除 <code>dp[T] = 0</code> 初始化且 <code>vis[T] = true</code> 外，其余点初始设为 <code>-INF</code>，这样只有真正能走到终点 <code>T</code> 的分支才会贡献合法最大值！"}
            }, "",
            "若题目不仅要求输出最长路径长度，还要输出字典序最小的路径节点序列，只需要在循环枚举邻居 <code>v</code>（或建边后给邻居按编号升序排序）时严格用 <code>&gt;</code>（不可用 <code>&gt;=</code>）刷新 <code>choice[u] = v</code> 即可！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

struct Edge { int to, w; };
vector<vector<Edge>> adj;
vector<int> dp, vis;

int DP(int u) {
    if (vis[u]) return dp[u];
    vis[u] = 1;
    for (int i = 0; i < static_cast<int>(adj[u].size()); ++i) {
        dp[u] = max(dp[u], DP(adj[u][i].to) + adj[u][i].w);
    }
    return dp[u];
}

int main() {
    int n = 5;
    adj.assign(n, {});
    dp.assign(n, 0);
    vis.assign(n, 0);
    adj[0] = {{1, 2}, {2, 5}}; adj[1] = {{3, 6}}; adj[2] = {{3, 1}, {4, 4}}; adj[3] = {{4, 3}};
    int ans = 0;
    for (int i = 0; i < n; ++i) ans = max(ans, DP(i));
    cout << "DAG longest path = " << ans << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_6", "练习：记忆化 DP 求 DAG 全局最长有向路径权值和", "completion",
            "输入有向无环图顶点数 n（0..n-1）与有向边数 m，随后 m 行 u v w（正整数边权）。求从任意点出发到任意点结束的最长路径总权值。",
            "5 5\n0 1 2\n0 2 5\n1 3 6\n2 3 1\n3 4 3", "11",
            R"JSON([{"label":"5点DAG最长路","input":"5 5\n0 1 2\n0 2 5\n1 3 6\n2 3 1\n3 4 3","expected":"11"},{"label":"无边零路径","input":"3 0","expected":"0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

struct Edge { int to, w; };

int main() {
    int n, m; cin >> n >> m;
    vector<vector<Edge>> adj(n);
    for (int i = 0; i < m; ++i) { int u, v, w; cin >> u >> v >> w; adj[u].push_back({v, w}); }
    // TODO: 编写记忆化搜索 DP(u) 计算从任意起点出发的最长路径长度
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

struct Edge { int to, w; };
vector<vector<Edge>> adj;
vector<int> dp, vis;

int DP(int u) {
    if (vis[u]) return dp[u];
    vis[u] = 1;
    for (int i = 0; i < static_cast<int>(adj[u].size()); ++i) {
        dp[u] = max(dp[u], DP(adj[u][i].to) + adj[u][i].w);
    }
    return dp[u];
}

int main() {
    int n, m; cin >> n >> m;
    adj.assign(n, {});
    dp.assign(n, 0);
    vis.assign(n, 0);
    for (int i = 0; i < m; ++i) { int u, v, w; cin >> u >> v >> w; adj[u].push_back({v, w}); }
    int ans = 0;
    for (int i = 0; i < n; ++i) ans = max(ans, DP(i));
    cout << ans << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "11.7"; l.chapter_id = "ch11"; l.sort_order = 7;
        l.title = "11.7 背包问题：01 背包（逆序一维）与完全背包（正序一维）";
        l.source_ref = "《算法笔记》11.7.1-11.7.2"; l.time_comp = "O(N*V)"; l.space_comp = "O(V)";
        l.viz_type = "dp"; l.viz_label = "01 背包内层 v 倒序防重复 vs 完全背包正序允许多次选取";
        l.tutorial_html = make_tut(
            "<strong>背包九讲核心一维压缩口诀（只需记住容量循环的方向！）：</strong>",
            {
                {"01 背包（每件最多选 1 次）→ 倒序", "<code>for (int v = V; v &gt;= w[i]; --v) dp[v] = max(dp[v], dp[v - w[i]] + c[i]);</code><br>因为从大到小更新时，用到的 <code>dp[v - w[i]]</code> 还是上一件物品未放入的旧值，确保第 <code>i</code> 件绝不被放两次！"},
                {"完全背包（每件可无限次选）→ 正序", "<code>for (int v = w[i]; v &lt;= V; ++v) dp[v] = max(dp[v], dp[v - w[i]] + c[i]);</code><br>从小到大更新时，<code>dp[v - w[i]]</code> 已包含“放入若干次第 <code>i</code> 件”的本层新值，自然实现无限次放入！"}
            }, "",
            "若题目要求“<strong>恰好装满背包</strong>时的最大价值”，初始化必须设 <code>dp[0] = 0</code>，其余 <code>dp[1..V] = -INF</code>；若只要求“不超过容量时的最大价值”，初始化全设为 <code>0</code> 即可。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int V = 8;
    vector<int> w = {3, 5, 1, 2, 2}, c = {4, 5, 2, 1, 3};
    vector<int> dp01(V + 1, 0), dpComp(V + 1, 0);
    for (int i = 0; i < static_cast<int>(w.size()); ++i) {
        for (int v = V; v >= w[i]; --v) dp01[v] = max(dp01[v], dp01[v - w[i]] + c[i]);
        for (int v = w[i]; v <= V; ++v) dpComp[v] = max(dpComp[v], dpComp[v - w[i]] + c[i]);
    }
    cout << "01=" << dp01[V] << " Complete=" << dpComp[V] << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_7_1", "练习 1：01 背包最大价值（滚动数组逆序）", "completion",
            "输入物品数量 N 与背包容量 V，随后 N 行给出体积 w_i 与价值 c_i，每种物品最多选 1 次，输出最大总价值。",
            "5 8\n3 4\n5 5\n1 2\n2 1\n2 3", "10",
            R"JSON([{"label":"《算法笔记》01背包样例","input":"5 8\n3 4\n5 5\n1 2\n2 1\n2 3","expected":"10"},{"label":"容量不够","input":"2 2\n5 10\n4 8","expected":"0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n, V; cin >> n >> V;
    vector<int> w(n), c(n);
    for (int i = 0; i < n; ++i) cin >> w[i] >> c[i];
    vector<int> dp(V + 1, 0);
    // TODO: 编写 01 背包状态转移（注意内层容量循环顺序）
    cout << dp[V] << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n, V; cin >> n >> V;
    vector<int> w(n), c(n);
    for (int i = 0; i < n; ++i) cin >> w[i] >> c[i];
    vector<int> dp(V + 1, 0);
    for (int i = 0; i < n; ++i)
        for (int v = V; v >= w[i]; --v)
            dp[v] = max(dp[v], dp[v - w[i]] + c[i]);
    cout << dp[V] << "\n";
    return 0;
})CPP");
        add_ex(l, "ex_11_7_2", "练习 2：完全背包最大价值（滚动数组正序）", "completion",
            "输入物品种类 N 与背包容量 V，随后 N 行给出体积 w_i 与价值 c_i，每种物品可选无限多次，输出最大总价值。",
            "3 10\n3 4\n4 5\n5 7", "14",
            R"JSON([{"label":"可重复选","input":"3 10\n3 4\n4 5\n5 7","expected":"14"},{"label":"单件凑满","input":"1 9\n3 5","expected":"15"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n, V; cin >> n >> V;
    vector<int> w(n), c(n);
    for (int i = 0; i < n; ++i) cin >> w[i] >> c[i];
    vector<int> dp(V + 1, 0);
    // TODO: 编写完全背包状态转移（注意内层容量正序循环）
    cout << dp[V] << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n, V; cin >> n >> V;
    vector<int> w(n), c(n);
    for (int i = 0; i < n; ++i) cin >> w[i] >> c[i];
    vector<int> dp(V + 1, 0);
    for (int i = 0; i < n; ++i)
        for (int v = w[i]; v <= V; ++v)
            dp[v] = max(dp[v], dp[v - w[i]] + c[i]);
    cout << dp[V] << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "11.8"; l.chapter_id = "ch11"; l.sort_order = 8;
        l.title = "11.8 DP 总结与高阶模型：区间 DP（石子合并）、状压与树形 DP";
        l.source_ref = "《算法笔记》11.8 · 提高集训高阶 DP"; l.time_comp = "O(n^3)"; l.space_comp = "O(n^2)";
        l.viz_type = "dp"; l.viz_label = "区间 DP 按区间长度 len=2..n 自短到长合并分裂点 k";
        l.tutorial_html = make_tut(
            "<strong>在《算法笔记》11.8 对“状态与转移”的总结上，集训常考三大高阶 DP：</strong>",
            {
                {"区间 DP：石子合并模型", "把一段区间 <code>[l, r]</code> 从中间分界 <code>k</code> 拆成左 <code>[l, k]</code> 与右 <code>[k+1, r]</code>，外层必须枚举区间长度 <code>len = 2..n</code>：<br><code>dp[l][r] = min(dp[l][r], dp[l][k] + dp[k+1][r] + pre[r] - pre[l-1]);</code>"},
                {"状压 DP 与树形 DP 概要", "① <strong>状态压缩 DP</strong>：当集合大小 <code>n &lt;= 20</code> 时，用一个二进制整数 <code>mask</code>（位掩码）代替布尔数组充当 DP 下标（如 TSP 旅行商最短路）；<br>② <strong>树形 DP</strong>：在树上按后序遍历先算完所有儿子 <code>dp[v]</code>，回溯汇总到父亲 <code>dp[u]</code>（如树上最大独立集）。"}
            }, "",
            "求最小花费类的区间 DP 前，所有非对角线元素 <code>dp[l][r] (l &lt; r)</code> 必须先初始化为极大值 <code>INF</code>，而单堆石子不用合并，务必设对角线 <code>dp[i][i] = 0</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    vector<int> a = {1, 3, 5, 2};
    int n = a.size();
    vector<int> pre(n + 1, 0);
    for (int i = 1; i <= n; ++i) pre[i] = pre[i - 1] + a[i - 1];
    vector<vector<int>> dp(n + 1, vector<int>(n + 1, 1e9));
    for (int i = 1; i <= n; ++i) dp[i][i] = 0;
    for (int len = 2; len <= n; ++len) {
        for (int l = 1; l + len - 1 <= n; ++l) {
            int r = l + len - 1;
            int cost = pre[r] - pre[l - 1];
            for (int k = l; k < r; ++k)
                dp[l][r] = min(dp[l][r], dp[l][k] + dp[k + 1][r] + cost);
        }
    }
    cout << "min merge cost = " << dp[1][n] << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_11_8", "练习：区间 DP 求石子合并最小总代价", "completion",
            "输入 n 堆石子重量，每次只能合并相邻两堆石子，代价为新堆总重量，求把所有石子合并成一堆的最小总代价。",
            "4\n1 3 5 2", "22",
            R"JSON([{"label":"4堆石子合并","input":"4\n1 3 5 2","expected":"22"},{"label":"1堆无需合并","input":"1\n10","expected":"0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<int> pre(n + 1, 0);
    for (int i = 1; i <= n; ++i) { int x; cin >> x; pre[i] = pre[i - 1] + x; }
    vector<vector<int>> dp(n + 1, vector<int>(n + 1, 1e9));
    for (int i = 1; i <= n; ++i) dp[i][i] = 0;
    // TODO: 枚举 len=2..n, 左端点 l, 分界点 k 转移并输出 dp[1][n]
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

int main() {
    int n; cin >> n;
    vector<int> pre(n + 1, 0);
    for (int i = 1; i <= n; ++i) { int x; cin >> x; pre[i] = pre[i - 1] + x; }
    vector<vector<int>> dp(n + 1, vector<int>(n + 1, 1e9));
    for (int i = 1; i <= n; ++i) dp[i][i] = 0;
    for (int len = 2; len <= n; ++len) {
        for (int l = 1; l + len - 1 <= n; ++l) {
            int r = l + len - 1;
            int cost = pre[r] - pre[l - 1];
            for (int k = l; k < r; ++k)
                dp[l][r] = min(dp[l][r], dp[l][k] + dp[k + 1][r] + cost);
        }
    }
    cout << dp[1][n] << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 12 章｜提高篇（6）——字符串专题（《算法笔记》Ch12 全 2 节）
    // =========================================================================
    chapters.push_back({"ch12", 12, "图论/DP/高阶(Ch10-13)", "第 12 章｜字符串进阶：字符串 Hash 与 KMP 算法（12.1-12.2）", "《算法笔记》Ch12 · 12.1-12.2 原书全 2 节"});

    {
        DsLesson l; l.id = "12.1"; l.chapter_id = "ch12"; l.sort_order = 1;
        l.title = "12.1 字符串 Hash 进阶：P 进制自然溢出与 O(1) 任意子串比对";
        l.source_ref = "《算法笔记》12.1"; l.time_comp = "O(n)"; l.space_comp = "O(n)";
        l.viz_type = "kmp"; l.viz_label = "把字符串当作 P=131 进制大整数做前缀 Hash：O(1) 提取子串指纹";
        l.tutorial_html = make_tut(
            "<strong>为什么《算法笔记》强推字符串 Hash？因为它能让你用极少的代码在 O(1) 时间内判断两个子串是否完全相等！</strong>",
            {
                {"多项式滚动前缀 Hash 公式", "取基底 <code>P = 131</code>（或 <code>13331</code>），用 <code>unsigned long long</code>（自动对 2<sup>64</sup> 自然溢出）或模大质数 <code>MOD = 1000000007</code>：<br>① 预处理：<code>H[i] = H[i - 1] * P + s[i]; pw[i] = pw[i - 1] * P;</code><br>② 任意子串 <code>S[l..r]</code> 的 Hash 值（1-indexed）：<code>hashLR = H[r] - H[l - 1] * pw[r - l + 1];</code>"}
            }, "",
            "计算单个字符贡献时切记不要把 <code>'a'</code> 映射到数字 <code>0</code>，否则字符串 <code>\"a\"</code>、<code>\"aa\"</code>、<code>\"aaa\"</code> 的 Hash 值全都会变成 <code>0</code>！直接加 <code>s[i]</code>（ASCII 码均非 0）或 <code>s[i] - 'a' + 1</code> 即可避免！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <string>
using namespace std;

using ull = unsigned long long;

int main() {
    string s = "abracadabra";
    int n = s.size();
    const ull P = 131;
    vector<ull> H(n + 1, 0), pw(n + 1, 1);
    for (int i = 1; i <= n; ++i) {
        H[i] = H[i - 1] * P + static_cast<ull>(s[i - 1]);
        pw[i] = pw[i - 1] * P;
    }
    auto getHash = [&](int l, int r) -> ull { return H[r] - H[l - 1] * pw[r - l + 1]; };
    cout << "sub[1..4]==sub[8..11]? " << (getHash(1, 4) == getHash(8, 11) ? "YES" : "NO") << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_12_1", "练习：滚动 Hash 快速统计字符串中长度为 L 的不同子串个数", "completion",
            "输入字符串 s (1 <= |s| <= 2000) 与整数 L (1 <= L <= |s|)，利用 P=131 进制前缀 Hash 计算所有长度为 L 的连续子串中互不相同的子串种数。",
            "ababcab\n2", "4",
            R"JSON([{"label":"长为2去重","input":"ababcab\n2","expected":"4"},{"label":"全相同字符","input":"aaaaa\n3","expected":"1"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

using ull = unsigned long long;

int main() {
    string s; int L;
    cin >> s >> L;
    // TODO: 计算前缀 H[] 与 pw[]，把所有长 L 子串的 Hash 收集、排序、unique 去重计数
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

using ull = unsigned long long;

int main() {
    string s; int L;
    cin >> s >> L;
    int n = s.size();
    const ull P = 131;
    vector<ull> H(n + 1, 0), pw(n + 1, 1);
    for (int i = 1; i <= n; ++i) {
        H[i] = H[i - 1] * P + static_cast<ull>(s[i - 1]);
        pw[i] = pw[i - 1] * P;
    }
    vector<ull> vals;
    for (int l = 1; l + L - 1 <= n; ++l) {
        int r = l + L - 1;
        vals.push_back(H[r] - H[l - 1] * pw[L]);
    }
    sort(vals.begin(), vals.end());
    vals.erase(unique(vals.begin(), vals.end()), vals.end());
    cout << vals.size() << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "12.2"; l.chapter_id = "ch12"; l.sort_order = 2;
        l.title = "12.2 KMP 算法：next[] 前缀函数、nextval 与 O(n+m) 匹配";
        l.source_ref = "《算法笔记》12.2"; l.time_comp = "O(n + m)"; l.space_comp = "O(m)";
        l.viz_type = "kmp"; l.viz_label = "KMP 前缀函数 pi[i]：失配时文本串指针 i 不回退，模式串 j = pi[j-1]";
        l.tutorial_html = make_tut(
            "<strong>KMP 算法的精髓在于主串指针 <code>i</code> 永不回退：</strong>遇到不匹配时，仅让模式串指针 <code>j</code> 根据预先求好的 <code>next</code> 数组退回已匹配的最长相等真前后缀位置！",
            {
                {"前缀函数 pi[i] (现代最简洁等价写法)", "<code>pi[i]</code> 定义为 <code>P[0..i]</code> 的最长相等真前后缀长度（即《算法笔记》中 <code>next[i] + 1</code>）：<br>• 预处理模式串 <code>P</code>（与匹配过程代码几乎完全一样！）：<code>while (j &gt; 0 &amp;&amp; P[i] != P[j]) j = pi[j - 1]; if (P[i] == P[j]) ++j; pi[i] = j;</code><br>• 匹配全长成功：当 <code>j == m</code> 时成功找到匹配，别忘了接着令 <code>j = pi[j - 1];</code> 继续寻找下一个（允许重叠）！"}
            }, "",
            "当一次完全匹配发生（<code>j == m</code>）准备找下一处出现时，必须写 <code>j = pi[j - 1];</code> 而不是粗暴归零 <code>j = 0;</code>！否则遇到主串 <code>\"abababa\"</code>、模式串 <code>\"aba\"</code> 就会漏掉中间重叠的那次匹配！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <string>
using namespace std;

int main() {
    string s = "abababa", p = "aba";
    int n = s.size(), m = p.size();
    vector<int> pi(m, 0);
    for (int i = 1, j = 0; i < m; ++i) {
        while (j > 0 && p[i] != p[j]) j = pi[j - 1];
        if (p[i] == p[j]) ++j;
        pi[i] = j;
    }
    int cnt = 0;
    for (int i = 0, j = 0; i < n; ++i) {
        while (j > 0 && s[i] != p[j]) j = pi[j - 1];
        if (s[i] == p[j]) ++j;
        if (j == m) { ++cnt; j = pi[j - 1]; }
    }
    cout << "match count = " << cnt << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_12_2", "练习：KMP 统计模式串在主串中出现次数（含重叠）", "completion",
            "输入主串 S 与模式串 P，使用 KMP 计算模式串 P 在主串 S 中总共出现了多少次（允许位置重叠）。",
            "abababa\naba", "3",
            R"JSON([{"label":"允许重叠匹配","input":"abababa\naba","expected":"3"},{"label":"完全无匹配","input":"abcdef\nxyz","expected":"0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
using namespace std;

int main() {
    string s, p; cin >> s >> p;
    // TODO: 构造模式串前缀函数 pi[] 并用 KMP 统计 s 中出现 p 的总次数
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <string>
using namespace std;

int main() {
    string s, p; cin >> s >> p;
    int n = s.size(), m = p.size();
    vector<int> pi(m, 0);
    for (int i = 1, j = 0; i < m; ++i) {
        while (j > 0 && p[i] != p[j]) j = pi[j - 1];
        if (p[i] == p[j]) ++j;
        pi[i] = j;
    }
    int cnt = 0;
    for (int i = 0, j = 0; i < n; ++i) {
        while (j > 0 && s[i] != p[j]) j = pi[j - 1];
        if (s[i] == p[j]) ++j;
        if (j == m) { ++cnt; j = pi[j - 1]; }
    }
    cout << cnt << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 13 章｜提高篇（7）——专题拓展（《算法笔记》Ch13 · 分块/树状数组/线段树）
    // =========================================================================
    chapters.push_back({"ch13", 13, "图论/DP/高阶(Ch10-13)", "第 13 章｜专题拓展：分块思想、树状数组 BIT 与线段树（13.1-13.3）", "《算法笔记》Ch13 · 13.1-13.3 全节"});

    {
        DsLesson l; l.id = "13.1"; l.chapter_id = "ch13"; l.sort_order = 1;
        l.title = "13.1 分块思想：Sqrt 根号分块 O(sqrt(N)) 查动态第 K 小";
        l.source_ref = "《算法笔记》13.1 · PAT A1057 Stack 原题框架"; l.time_comp = "O(Q sqrt(N))"; l.space_comp = "O(N)";
        l.viz_type = "bit"; l.viz_label = "把 [0..10^5] 切分成 316 个大块：每块顺次累加跳过，最后进目标块精扫";
        l.tutorial_html = make_tut(
            "<strong>分块是最朴素但也最万能的优雅暴力数据结构！</strong>对于值域 <code>10<sup>5</sup></code>，令每块长度 <code>B = &lfloor;&radic;100000&rfloor; = 316</code>，把值域分成约 <code>317</code> 块：",
            {
                {"《算法笔记》经典第 K 小模板 (PAT A1057)", "• 插入数值 <code>x</code>：同时更新散点桶 <code>table[x]++</code> 与所属整块总计 <code>block[x / 316]++</code>（仅 <code>O(1)</code>！）；<br>• 查询全局第 <code>K</code> 小数：① 整块遍历 <code>for (idx = 0; sum + block[idx] &lt; K; ++idx) sum += block[idx];</code>（最多 317 次）；② 进入该块内枚举 <code>num = idx * 316</code> 起步，<code>while (sum + table[num] &lt; K) sum += table[num++];</code>（同样最多 316 次）！"}
            }, "",
            "分块实现比平衡树/树状数组+二分短得多且绝不容易写挂；注意分块长度选 <code>&radic;N</code> 才能使跨块与块内扫描步数完美平衡在 300 步以内！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
using namespace std;

const int MAXV = 100005;
const int B = 316;
int tableCnt[MAXV] = {0}, blockCnt[320] = {0};

void addVal(int x, int d) { tableCnt[x] += d; blockCnt[x / B] += d; }

int kthSmallest(int K) {
    int sum = 0, idx = 0;
    while (sum + blockCnt[idx] < K) { sum += blockCnt[idx]; ++idx; }
    int num = idx * B;
    while (sum + tableCnt[num] < K) { sum += tableCnt[num]; ++num; }
    return num;
}

int main() {
    for (int x : {15, 3, 20, 7, 10}) addVal(x, 1);
    cout << "3rd smallest = " << kthSmallest(3) << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_13_1", "练习：分块法维护动态集合插入、删除与第 K 小查询", "completion",
            "输入操作总数 q，随后 q 行操作：`1 x` 插入整数 x (1 <= x <= 100000)；`2 x` 删除一个 x；`3 k` 查询并输出当前集合第 k 小元素（保证操作均合法）。",
            "7\n1 15\n1 3\n1 20\n1 7\n3 2\n2 7\n3 2", "7\n15",
            R"JSON([{"label":"插删与第k小查询","input":"7\n1 15\n1 3\n1 20\n1 7\n3 2\n2 7\n3 2","expected":"7\n15"},{"label":"含重复元素","input":"4\n1 5\n1 5\n3 1\n3 2","expected":"5\n5"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

const int MAXV = 100005, B = 316;
int tableCnt[MAXV] = {0}, blockCnt[320] = {0};

int main() {
    int q; cin >> q;
    // TODO: 实现分块插入/删除与第 k 小查询
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

const int MAXV = 100005, B = 316;
int tableCnt[MAXV] = {0}, blockCnt[320] = {0};

int main() {
    int q; cin >> q;
    while (q--) {
        int op, x; cin >> op >> x;
        if (op == 1) { tableCnt[x]++; blockCnt[x / B]++; }
        else if (op == 2) { tableCnt[x]--; blockCnt[x / B]--; }
        else if (op == 3) {
            int sum = 0, idx = 0;
            while (sum + blockCnt[idx] < x) { sum += blockCnt[idx]; ++idx; }
            int num = idx * B;
            while (sum + tableCnt[num] < x) { sum += tableCnt[num]; ++num; }
            cout << num << "\n";
        }
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "13.2"; l.chapter_id = "ch13"; l.sort_order = 2;
        l.title = "13.2 树状数组 (BIT)：lowbit(x) = x & (-x)、单点修改与前缀求和";
        l.source_ref = "《算法笔记》13.2"; l.time_comp = "O(log n)"; l.space_comp = "O(n)";
        l.viz_type = "bit"; l.viz_label = "树状数组 lowbit(x) = x & (-x) 二进制管辖区间跳跃";
        l.tutorial_html = make_tut(
            "<strong>树状数组 (Binary Indexed Tree / Fenwick Tree) 是竞赛里代码量最惊艳的动态区间前缀和利器，核心加起来只有 10 行！</strong>",
            {
                {"两大 O(log n) 核心动作（下标严格从 1 开始！）", "• <code>inline int lowbit(int x) { return x &amp; (-x); }</code><br>• <strong>单点加 delta (向上更新父节点)</strong>：<code>for (int i = x; i &lt;= n; i += lowbit(i)) c[i] += delta;</code><br>• <strong>求前缀和 sum[1..x] (向下摘区间)</strong>：<code>for (int i = x; i &gt; 0; i -= lowbit(i)) res += c[i];</code><br>• <strong>任意区间 [l, r] 和</strong>：<code>query(r) - query(l - 1)</code>！"}
            }, "",
            "树状数组下标<strong>绝对必须从 <code>1</code> 开始</strong>！若粗心传进 <code>i = 0</code>，因为 <code>lowbit(0) == 0</code>，<code>for (int i = 0; i <= n; i += lowbit(i))</code> 会直接无限死循环！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
using namespace std;

struct BIT {
    int n; vector<long long> c;
    explicit BIT(int n) : n(n), c(n + 1, 0) {}
    static int lowbit(int x) { return x & (-x); }
    void add(int x, long long d) { for (; x <= n; x += lowbit(x)) c[x] += d; }
    long long query(int x) const { long long s = 0; for (; x > 0; x -= lowbit(x)) s += c[x]; return s; }
    long long rangeSum(int l, int r) const { return query(r) - query(l - 1); }
};

int main() {
    BIT bit(5);
    int init[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; ++i) bit.add(i + 1, init[i]);
    cout << "sum[2..4] = " << bit.rangeSum(2, 4) << "\n";
    bit.add(3, 10);
    cout << "after add(3,10) sum[2..4] = " << bit.rangeSum(2, 4) << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_13_2", "练习：树状数组实现单点增加与区间求和", "completion",
            "输入长度 n 与操作数 q，第一行输入初始数组 a[1..n]。接着 q 行操作：`1 idx delta` 表示 a[idx] += delta；`2 l r` 查询并输出区间 [l, r] 元素和。",
            "5 3\n1 2 3 4 5\n2 2 4\n1 3 10\n2 2 4", "9\n19",
            R"JSON([{"label":"单点修改+区间求和","input":"5 3\n1 2 3 4 5\n2 2 4\n1 3 10\n2 2 4","expected":"9\n19"},{"label":"负数修改","input":"3 2\n10 10 10\n1 2 -5\n2 1 3","expected":"25"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

int main() {
    int n, q; cin >> n >> q;
    vector<long long> c(n + 1, 0);
    // TODO: 读入初始值建 BIT，处理 1(add) 与 2(query r - query l-1)
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

int main() {
    int n, q; cin >> n >> q;
    vector<long long> c(n + 1, 0);
    auto lowbit = [](int x) { return x & (-x); };
    auto add = [&](int x, long long d) { for (; x <= n; x += lowbit(x)) c[x] += d; };
    auto query = [&](int x) { long long s = 0; for (; x > 0; x -= lowbit(x)) s += c[x]; return s; };
    for (int i = 1; i <= n; ++i) { long long v; cin >> v; add(i, v); }
    while (q--) {
        int op; cin >> op;
        if (op == 1) { int idx; long long d; cin >> idx >> d; add(idx, d); }
        else { int l, r; cin >> l >> r; cout << query(r) - query(l - 1) << "\n"; }
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "13.3"; l.chapter_id = "ch13"; l.sort_order = 3;
        l.title = "13.3 线段树 (Segment Tree)：Lazy Tag 懒标记区间加与区间查询";
        l.source_ref = "《算法笔记》集训高阶 · 线段树通用模板"; l.time_comp = "O(log n)"; l.space_comp = "O(4n)";
        l.viz_type = "bit"; l.viz_label = "二叉树区间分治：pushUp 上浮汇总 & pushDown 下传懒标记";
        l.tutorial_html = make_tut(
            "<strong>当需要“区间整体修改 + 区间求和/极值/各种不可减信息（如区间最大值）”时，请出数据结构重型兵器——线段树！</strong>",
            {
                {"线段树四大核心函数", "① <strong><code>pushUp(p)</code></strong>：<code>sum[p] = sum[2*p] + sum[2*p+1];</code><br>② <strong><code>apply(p, l, r, val)</code></strong>：只改当前完整覆盖节点 <code>sum[p] += val * (r - l + 1); lazy[p] += val;</code><br>③ <strong><code>pushDown(p, l, r)</code></strong>：凡是要递归进子节点之前，如果 <code>lazy[p] != 0</code>，立即下发给左右半区并清空 <code>lazy[p] = 0</code>！<br>④ <strong><code>update / query</code></strong>：完整被查询区间包住就直接用整块答案返回，否则先 <code>pushDown</code> 再按 <code>mid</code> 分治左右半区！"}
            }, "",
            "堆式存储线段树的数组空间<strong>务必开到 <code>4 * n</code></strong>（而不是 <code>2 * n</code>），因为最底层可能有一部分下标落到接近 <code>4n</code> 的位置；另外区间乘加计算记得全面开 <code>long long</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
using namespace std;

struct SegTree {
    int n; vector<long long> sum, lazy;
    explicit SegTree(const vector<long long>& a) : n(a.size() - 1), sum(4 * n + 4, 0), lazy(4 * n + 4, 0) {
        build(1, 1, n, a);
    }
    void build(int p, int l, int r, const vector<long long>& a) {
        if (l == r) { sum[p] = a[l]; return; }
        int m = (l + r) / 2;
        build(p * 2, l, m, a); build(p * 2 + 1, m + 1, r, a);
        sum[p] = sum[p * 2] + sum[p * 2 + 1];
    }
    void pushDown(int p, int l, int r) {
        if (lazy[p] == 0) return;
        int m = (l + r) / 2;
        sum[p * 2] += lazy[p] * (m - l + 1); lazy[p * 2] += lazy[p];
        sum[p * 2 + 1] += lazy[p] * (r - m); lazy[p * 2 + 1] += lazy[p];
        lazy[p] = 0;
    }
    void update(int p, int l, int r, int ql, int qr, long long v) {
        if (ql <= l && r <= qr) { sum[p] += v * (r - l + 1); lazy[p] += v; return; }
        pushDown(p, l, r);
        int m = (l + r) / 2;
        if (ql <= m) update(p * 2, l, m, ql, qr, v);
        if (qr > m) update(p * 2 + 1, m + 1, r, ql, qr, v);
        sum[p] = sum[p * 2] + sum[p * 2 + 1];
    }
    long long query(int p, int l, int r, int ql, int qr) {
        if (ql <= l && r <= qr) return sum[p];
        pushDown(p, l, r);
        int m = (l + r) / 2;
        long long res = 0;
        if (ql <= m) res += query(p * 2, l, m, ql, qr);
        if (qr > m) res += query(p * 2 + 1, m + 1, r, ql, qr);
        return res;
    }
};

int main() {
    vector<long long> a = {0, 1, 5, 4, 2, 3};
    SegTree st(a);
    st.update(1, 1, 5, 2, 4, 2);
    cout << "query[3..5] = " << st.query(1, 1, 5, 3, 5) << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_13_3", "练习：线段树 Lazy Tag 实现区间加与区间求和", "completion",
            "输入长度 n 与操作数 q，第一行输入初始数组 a[1..n]。随后 q 行操作：`1 l r v` 将区间 [l, r] 每个数加上 v；`2 l r` 查询并输出区间 [l, r] 元素和。",
            "5 4\n1 5 4 2 3\n2 1 3\n1 2 4 2\n2 1 3\n2 3 5", "10\n14\n13",
            R"JSON([{"label":"区间加+区间求和","input":"5 4\n1 5 4 2 3\n2 1 3\n1 2 4 2\n2 1 3\n2 3 5","expected":"10\n14\n13"},{"label":"单元素区间","input":"2 2\n10 20\n1 1 2 -5\n2 1 1","expected":"5"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

// TODO: 实现带 Lazy Tag 懒标记的线段树（空间开 4*n），处理区间加与区间求和
int main() {
    int n, q; cin >> n >> q;
    vector<long long> a(n + 1);
    for (int i = 1; i <= n; ++i) cin >> a[i];
    return 0;
}
)CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

vector<long long> a, sum, lazy;

void build(int p, int l, int r) {
    if (l == r) { sum[p] = a[l]; return; }
    int m = (l + r) / 2;
    build(p * 2, l, m);
    build(p * 2 + 1, m + 1, r);
    sum[p] = sum[p * 2] + sum[p * 2 + 1];
}

void pushDown(int p, int l, int r) {
    if (lazy[p] == 0) return;
    int m = (l + r) / 2;
    sum[p * 2] += lazy[p] * (m - l + 1); lazy[p * 2] += lazy[p];
    sum[p * 2 + 1] += lazy[p] * (r - m); lazy[p * 2 + 1] += lazy[p];
    lazy[p] = 0;
}

void update(int p, int l, int r, int ql, int qr, long long v) {
    if (ql <= l && r <= qr) { sum[p] += v * (r - l + 1); lazy[p] += v; return; }
    pushDown(p, l, r);
    int m = (l + r) / 2;
    if (ql <= m) update(p * 2, l, m, ql, qr, v);
    if (qr > m) update(p * 2 + 1, m + 1, r, ql, qr, v);
    sum[p] = sum[p * 2] + sum[p * 2 + 1];
}

long long query(int p, int l, int r, int ql, int qr) {
    if (ql <= l && r <= qr) return sum[p];
    pushDown(p, l, r);
    int m = (l + r) / 2;
    long long res = 0;
    if (ql <= m) res += query(p * 2, l, m, ql, qr);
    if (qr > m) res += query(p * 2 + 1, m + 1, r, ql, qr);
    return res;
}

int main() {
    int n, q; cin >> n >> q;
    a.assign(n + 1, 0); sum.assign(4 * n + 4, 0); lazy.assign(4 * n + 4, 0);
    for (int i = 1; i <= n; ++i) cin >> a[i];
    build(1, 1, n);
    while (q--) {
        int op, l, r; cin >> op >> l >> r;
        if (op == 1) { long long v; cin >> v; update(1, 1, n, l, r, v); }
        else cout << query(1, 1, n, l, r) << "\n";
    }
    return 0;
}
)CPP");
        lessons.push_back(std::move(l));
    }
}

} // namespace ds_seed
} // namespace omni
