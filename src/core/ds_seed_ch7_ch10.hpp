#pragma once

#include "ds_seed_common.hpp"

namespace omni {
namespace ds_seed {

inline void seed_ch7_ch10(std::vector<DsChapter>& chapters, std::vector<DsLesson>& lessons) {
    // =========================================================================
    // 第 7 章｜提高篇（1）——数据结构专题（一）：栈、队列与链表（《算法笔记》Ch7）
    // =========================================================================
    chapters.push_back({"ch7", 7, "数据结构与搜索(Ch7-9)", "第 7 章｜栈、队列、单链表与静态链表（《算法笔记》Ch7 全节）", "《算法笔记》Ch7 · 7.1-7.3 原书全节 + 单调栈/队列/跳表"});

    {
        DsLesson l; l.id = "7.1"; l.chapter_id = "ch7"; l.sort_order = 1;
        l.title = "7.1 栈的应用：数组栈 vs std::stack 与简单计算器";
        l.source_ref = "《算法笔记》7.1.1-7.1.2"; l.time_comp = "O(n)"; l.space_comp = "O(n)";
        l.viz_type = "stack"; l.viz_label = "栈的后进先出 (LIFO)：中缀表达式转后缀 / 双栈求值";
        l.tutorial_html = make_tut(
            "<strong>栈（Stack）的核心约束是后进先出（LIFO）：</strong>在纯 C 环境下只需一个数组与栈顶下标 <code>int st[MAXN], top = -1;</code>；在 C++ 中可直接用 <code>std::stack&lt;T&gt;</code>。竞赛高频综合应用是<strong>简单计算器</strong>（中缀表达式求值）。",
            {
                {"C 数组栈 vs std::stack", "纯 C：入栈 <code>st[++top] = x;</code>，取顶 <code>st[top]</code>，出栈 <code>top--</code>，判空 <code>top == -1</code>；<br>C++ STL：<code>push(x)</code>、<code>top()</code>、<code>pop()</code>、<code>empty()</code>。"},
                {"《算法笔记》中缀转后缀（逆波兰）两步法", "① 从左到右扫描中缀串，遇到<strong>数字</strong>直接加入后缀队列；② 遇到<strong>操作符</strong>：若其优先级<strong>低于或等于</strong>操作符栈顶（<code>(</code> 除外），不断弹出栈顶进队列，最后将该操作符压栈。左括号直接入栈，右括号一直弹到左括号为止。"},
                {"括号与双栈直接求值（更直观）", "也可以维护数值栈 <code>vals</code> 与符号栈 <code>ops</code>：每当需要弹出栈顶运算符 <code>op</code> 时，从 <code>vals</code> 连续弹出右操作数 <code>b</code>、左操作数 <code>a</code> 计算 <code>a op b</code> 压回 <code>vals</code>。"}
            },
            "<div class=\"ds-note-box\"><strong>优先级记忆表：</strong><code>(</code> 在栈内最低（不主动弹栈），<code>+ / -</code> 为 1，<code>* / /</code> 为 2。同级运算符（例如 <code>a - b + c</code>）满足“栈顶优先级 <code>&gt;=</code> 当前”时必须先弹栈，保证从左到右结合！</div>",
            "读入整行算式前若刚读完整数，要用 <code>getchar()</code> 或 <code>ws</code> 吞掉换行；另外连续多位数字需用 <code>num = num * 10 + (s[i] - '0')</code> 拼出完整整数！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <stack>
#include <string>
#include <cctype>
using namespace std;

int prec(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

void evalTop(stack<long long>& vals, stack<char>& ops) {
    long long b = vals.top(); vals.pop();
    long long a = vals.top(); vals.pop();
    char op = ops.top(); ops.pop();
    if (op == '+') vals.push(a + b);
    else if (op == '-') vals.push(a - b);
    else if (op == '*') vals.push(a * b);
    else if (op == '/') vals.push(a / b);
}

int main() {
    string s = "3 + 4 * (2 + 5) - 6 / 2";
    stack<long long> vals;
    stack<char> ops;
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        if (s[i] == ' ') continue;
        if (isdigit(static_cast<unsigned char>(s[i]))) {
            long long num = 0;
            while (i < static_cast<int>(s.size()) && isdigit(static_cast<unsigned char>(s[i]))) {
                num = num * 10 + (s[i] - '0');
                ++i;
            }
            --i;
            vals.push(num);
        } else if (s[i] == '(') {
            ops.push('(');
        } else if (s[i] == ')') {
            while (!ops.empty() && ops.top() != '(') evalTop(vals, ops);
            if (!ops.empty()) ops.pop();
        } else {
            while (!ops.empty() && prec(ops.top()) >= prec(s[i])) evalTop(vals, ops);
            ops.push(s[i]);
        }
    }
    while (!ops.empty()) evalTop(vals, ops);
    cout << vals.top() << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_7_1", "练习：简单算术表达式求值（含括号四则运算）", "completion",
            "读入一行含非负整数、+ - * /、圆括号 ( ) 及空格的中缀表达式（除法为整数整除且除数非零），计算并输出表达式整数结果。",
            "1 + 2 * (3 + 4)", "15",
            R"JSON([{"label":"含括号混合运算","input":"1 + 2 * (3 + 4)","expected":"15"},{"label":"同级左结合减加","input":"10 - 3 + 2 * 5","expected":"17"},{"label":"嵌套括号整除","input":"(30 + 6) / (2 + 4)","expected":"6"}])JSON",
            R"CPP(#include <iostream>
#include <stack>
#include <string>
#include <cctype>
using namespace std;

int prec(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

int main() {
    string s;
    getline(cin, s);
    // TODO: 使用数值栈与符号栈计算带括号四则表达式结果并输出
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <stack>
#include <string>
#include <cctype>
using namespace std;

int prec(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

void evalTop(stack<long long>& vals, stack<char>& ops) {
    long long b = vals.top(); vals.pop();
    long long a = vals.top(); vals.pop();
    char op = ops.top(); ops.pop();
    if (op == '+') vals.push(a + b);
    else if (op == '-') vals.push(a - b);
    else if (op == '*') vals.push(a * b);
    else if (op == '/') vals.push(a / b);
}

int main() {
    string s;
    getline(cin, s);
    stack<long long> vals;
    stack<char> ops;
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        if (isspace(static_cast<unsigned char>(s[i]))) continue;
        if (isdigit(static_cast<unsigned char>(s[i]))) {
            long long num = 0;
            while (i < static_cast<int>(s.size()) && isdigit(static_cast<unsigned char>(s[i]))) {
                num = num * 10 + (s[i] - '0');
                ++i;
            }
            --i;
            vals.push(num);
        } else if (s[i] == '(') {
            ops.push('(');
        } else if (s[i] == ')') {
            while (!ops.empty() && ops.top() != '(') evalTop(vals, ops);
            if (!ops.empty()) ops.pop();
        } else {
            while (!ops.empty() && prec(ops.top()) >= prec(s[i])) evalTop(vals, ops);
            ops.push(s[i]);
        }
    }
    while (!ops.empty()) evalTop(vals, ops);
    cout << vals.top() << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "7.2"; l.chapter_id = "ch7"; l.sort_order = 2;
        l.title = "7.2 队列、单调栈与单调队列 (滑动窗口极值)";
        l.source_ref = "《算法笔记》7.2 · 算法集训大纲 单调栈/单调队列"; l.time_comp = "O(n)"; l.space_comp = "O(n)";
        l.viz_type = "queue"; l.viz_label = "双端队列维护定长窗口 [i-k+1, i] 最大值";
        l.tutorial_html = make_tut(
            "<strong>队列（Queue）先进先出（FIFO），延伸出竞赛最锋利的线性提速神器：</strong>① <strong>单调栈</strong>（找左边/右边第一个比我大或小的元素，每个元素最多进栈一次出栈一次，<code>O(n)</code>）；② <strong>单调队列</strong>（<code>deque</code>，维护滑动窗口最值）。",
            {
                {"C 数组队列 vs STL queue/deque", "普通顺序队用 <code>int q[N], front = 0, rear = -1;</code>；循环队下标取模 <code>(rear + 1) % CAP</code>。双端滑窗必须用 <code>std::deque&lt;int&gt;</code>，支持队头队尾同时 <code>O(1)</code> 弹出。"},
                {"单调队列三步（始终存下标！）", "① <strong>淘汰过期队头</strong>：<code>if (!dq.empty() &amp;&amp; dq.front() &lt;= i - k) dq.pop_front();</code><br>② <strong>维护队尾单调性</strong>：求最大值时保持队头到队尾严格递减，凡是 <code>a[dq.back()] &lt;= a[i]</code> 的老弱病残统统 <code>pop_back()</code>！<br>③ <code>dq.push_back(i);</code> 满窗口（<code>i &gt;= k - 1</code>）时输出 <code>a[dq.front()]</code>。"},
                {"单调栈一招通杀 Next Greater", "从左到右扫数组维护<strong>栈底到栈顶单调递减栈</strong>（存下标）：遇到 <code>a[i] &gt; a[st.top()]</code>，说明 <code>i</code> 正好是栈顶元素右侧第一个更大值，弹栈记录 <code>ans[st.top()] = i</code>！"}
            }, "",
            "队列与单调栈内部务必<strong>存数组下标而不是直接存元素值</strong>！因为没有下标就无法在滑动窗口右移时判断队头元素是否已经掉出窗口左边界 <code>i - k + 1</code>。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <deque>
using namespace std;

int main() {
    vector<int> a = {1, 3, -1, -3, 5, 3, 6, 7};
    int k = 3;
    deque<int> dq; // 存下标，对应元素从队头到队尾严格递减
    for (int i = 0; i < static_cast<int>(a.size()); ++i) {
        while (!dq.empty() && dq.front() <= i - k) dq.pop_front();
        while (!dq.empty() && a[dq.back()] <= a[i]) dq.pop_back();
        dq.push_back(i);
        if (i >= k - 1) cout << a[dq.front()] << " ";
    }
    cout << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_7_2_1", "练习 1：单调队列求滑动窗口最大值", "completion",
            "输入 n, k 与长度为 n 的整数数组，输出每个大小为 k 的连续滑动窗口最大值（空格分隔，末尾无多余空格）。",
            "8 3\n1 3 -1 -3 5 3 6 7", "3 3 5 5 6 7",
            R"JSON([{"label":"经典滑窗 8 3","input":"8 3\n1 3 -1 -3 5 3 6 7","expected":"3 3 5 5 6 7"},{"label":"k=1 原样输出","input":"4 1\n4 2 9 1","expected":"4 2 9 1"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <deque>
using namespace std;

int main() {
    int n, k;
    if (!(cin >> n >> k)) return 0;
    vector<int> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    // TODO: 用 deque 维护递减单调队列并输出每个窗口的最大值
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <deque>
using namespace std;

int main() {
    int n, k;
    if (!(cin >> n >> k)) return 0;
    vector<int> a(n);
    for (int i = 0; i < n; ++i) cin >> a[i];
    deque<int> dq;
    bool first = true;
    for (int i = 0; i < n; ++i) {
        while (!dq.empty() && dq.front() <= i - k) dq.pop_front();
        while (!dq.empty() && a[dq.back()] <= a[i]) dq.pop_back();
        dq.push_back(i);
        if (i >= k - 1) {
            if (!first) cout << " ";
            cout << a[dq.front()];
            first = false;
        }
    }
    cout << "\n";
    return 0;
})CPP");
        add_ex(l, "ex_7_2_2", "练习 2：单调栈求右侧首个更大元素下标", "completion",
            "输入 n 与 n 个整数 a[0..n-1]，对每个位置输出其右侧首个严格大于它的元素下标（0-indexed），不存在则输出 -1。",
            "5\n73 74 75 71 76", "1 2 4 4 -1",
            R"JSON([{"label":"递增弹出","input":"5\n73 74 75 71 76","expected":"1 2 4 4 -1"},{"label":"严格递减","input":"4\n9 7 5 3","expected":"-1 -1 -1 -1"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <stack>
using namespace std;

int main() {
    int n; cin >> n;
    vector<int> a(n), ans(n, -1);
    for (int i = 0; i < n; ++i) cin >> a[i];
    // TODO: 使用单调递减栈求每个位置右侧首个更大元素的下标
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <stack>
using namespace std;

int main() {
    int n; cin >> n;
    vector<int> a(n), ans(n, -1);
    for (int i = 0; i < n; ++i) cin >> a[i];
    stack<int> st;
    for (int i = 0; i < n; ++i) {
        while (!st.empty() && a[i] > a[st.top()]) {
            ans[st.top()] = i;
            st.pop();
        }
        st.push(i);
    }
    for (int i = 0; i < n; ++i) {
        cout << ans[i] << (i + 1 == n ? '\n' : ' ');
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "7.3A"; l.chapter_id = "ch7"; l.sort_order = 3;
        l.title = "7.3A 动态链表：malloc vs new、增删节点与三指针反转";
        l.source_ref = "《算法笔记》7.3.1-7.3.4"; l.time_comp = "O(n)"; l.space_comp = "O(1)";
        l.viz_type = "linked_list"; l.viz_label = "单链表三指针原地反转演示 (prev/curr/next)";
        l.tutorial_html = make_tut(
            "<strong>动态链表是从纯 C 手动内存管理过渡到 C++ 面向对象节点的最佳分水岭：</strong>在 C 语言里必须用 <code>&lt;stdlib.h&gt;</code> 的 <code>(Node*)malloc(sizeof(Node))</code> 申请、<code>free(p)</code> 释放；在 C++ 中用带构造函数的 <code>new Node(x)</code> 与 <code>delete p</code> 更加优雅安全。",
            {
                {"C 风格 vs C++ 风格建节点", "纯 C：<code>Node* p = (Node*)malloc(sizeof(Node)); p-&gt;data = x; p-&gt;next = NULL;</code><br>C++：<code>struct ListNode { int val; ListNode* next; ListNode(int v): val(v), next(nullptr) {} };</code>"},
                {"带头结点 (Dummy) 统一增删边界", "给链表加一个不存数据的哨兵 <code>ListNode dummy(0); dummy.next = head;</code>，删除所有等于 <code>x</code> 的节点或头插节点时再也不用特判 <code>head == NULL</code>！"},
                {"三指针原地反转 (O(1) 空间)", "<code>ListNode *prev = nullptr, *curr = head;</code><br><code>while (curr) { ListNode* nxt = curr-&gt;next; curr-&gt;next = prev; prev = curr; curr = nxt; }</code>，最后 <code>return prev;</code>。"},
                {"快慢指针定位中点/判环", "<code>slow</code> 走 1 步、<code>fast</code> 走 2 步，<code>while (fast &amp;&amp; fast-&gt;next)</code> 结束时 <code>slow</code> 正好在中点！"}
            }, "",
            "移动 <code>fast = fast-&gt;next-&gt;next</code> 前务必同时检查 <code>fast != nullptr &amp;&amp; fast-&gt;next != nullptr</code>，否则偶数长链表末尾必定触发空指针段错误！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <stdlib.h>

// 同时展示 C malloc/free 与 C++ new/delete 风格节点
struct ListNode {
    int val;
    ListNode* next;
    explicit ListNode(int v) : val(v), next(nullptr) {}
};

ListNode* reverseList(ListNode* head) {
    ListNode *prev = nullptr, *curr = head;
    while (curr != nullptr) {
        ListNode* nxt = curr->next;
        curr->next = prev;
        prev = curr;
        curr = nxt;
    }
    return prev;
}

int main() {
    ListNode dummy(0), *tail = &dummy;
    int arr[4] = {1, 2, 3, 4};
    for (int i = 0; i < 4; ++i) {
        tail->next = new ListNode(arr[i]);
        tail = tail->next;
    }
    ListNode* head = reverseList(dummy.next);
    for (ListNode* p = head; p != nullptr; p = p->next) {
        printf("%d -> ", p->val);
    }
    printf("NULL\n");
    while (head) { ListNode* t = head->next; delete head; head = t; }
    return 0;
})CPP";
        add_ex(l, "ex_7_3a_1", "练习 1：单链表原地反转 (reverseList)", "completion",
            "输入整数 n 与 n 个节点值，构造单链表后补全 reverseList 实现 O(1) 空间原地反转，输出反转结果（格式如 5 -> 4 -> NULL 或空链表 NULL）。",
            "5\n1 2 3 4 5", "5 -> 4 -> 3 -> 2 -> 1 -> NULL",
            R"JSON([{"label":"5 节点链表","input":"5\n1 2 3 4 5","expected":"5 -> 4 -> 3 -> 2 -> 1 -> NULL"},{"label":"单节点","input":"1\n42","expected":"42 -> NULL"},{"label":"空链表","input":"0","expected":"NULL"}])JSON",
            R"CPP(#include <iostream>
using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    explicit ListNode(int v) : val(v), next(nullptr) {}
};

ListNode* reverseList(ListNode* head) {
    ListNode *prev = nullptr, *curr = head;
    // TODO: 补全三指针循环实现原地反转并返回新头节点
    return prev;
}

int main() {
    int n; cin >> n;
    ListNode dummy(0), *tail = &dummy;
    for (int i = 0; i < n; ++i) {
        int x; cin >> x;
        tail->next = new ListNode(x);
        tail = tail->next;
    }
    ListNode* ans = reverseList(dummy.next);
    for (ListNode* p = ans; p; p = p->next) cout << p->val << " -> ";
    cout << "NULL\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    explicit ListNode(int v) : val(v), next(nullptr) {}
};

ListNode* reverseList(ListNode* head) {
    ListNode *prev = nullptr, *curr = head;
    while (curr != nullptr) {
        ListNode* nxt = curr->next;
        curr->next = prev;
        prev = curr;
        curr = nxt;
    }
    return prev;
}

int main() {
    int n; cin >> n;
    ListNode dummy(0), *tail = &dummy;
    for (int i = 0; i < n; ++i) {
        int x; cin >> x;
        tail->next = new ListNode(x);
        tail = tail->next;
    }
    ListNode* ans = reverseList(dummy.next);
    for (ListNode* p = ans; p; p = p->next) cout << p->val << " -> ";
    cout << "NULL\n";
    return 0;
})CPP");
        add_ex(l, "ex_7_3a_2", "练习 2：快慢指针定位链表中点值", "completion",
            "输入 n（>=1）个节点值构造单链表，使用快慢指针定位中间节点并输出其 val（偶数个节点返回靠右的那个中间节点）。",
            "6\n10 20 30 40 50 60", "40",
            R"JSON([{"label":"偶数长返回靠右中点","input":"6\n10 20 30 40 50 60","expected":"40"},{"label":"奇数长","input":"5\n1 3 5 7 9","expected":"5"}])JSON",
            R"CPP(#include <iostream>
using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    explicit ListNode(int v) : val(v), next(nullptr) {}
};

int middleValue(ListNode* head) {
    ListNode *slow = head, *fast = head;
    // TODO: 移动快慢指针，结束时返回 slow->val
    return slow->val;
}

int main() {
    int n; cin >> n;
    ListNode dummy(0), *tail = &dummy;
    for (int i = 0; i < n; ++i) {
        int x; cin >> x;
        tail->next = new ListNode(x);
        tail = tail->next;
    }
    cout << middleValue(dummy.next) << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
using namespace std;

struct ListNode {
    int val;
    ListNode* next;
    explicit ListNode(int v) : val(v), next(nullptr) {}
};

int middleValue(ListNode* head) {
    ListNode *slow = head, *fast = head;
    while (fast != nullptr && fast->next != nullptr) {
        slow = slow->next;
        fast = fast->next->next;
    }
    return slow->val;
}

int main() {
    int n; cin >> n;
    ListNode dummy(0), *tail = &dummy;
    for (int i = 0; i < n; ++i) {
        int x; cin >> x;
        tail->next = new ListNode(x);
        tail = tail->next;
    }
    cout << middleValue(dummy.next) << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "7.3B"; l.chapter_id = "ch7"; l.sort_order = 4;
        l.title = "7.3B 静态链表 Node[] 地址排序五步法与跳表";
        l.source_ref = "《算法笔记》7.3.5 · PAT 甲级经典模板"; l.time_comp = "O(n log n)"; l.space_comp = "O(N)";
        l.viz_type = "linked_list"; l.viz_label = "静态链表 Node[100005] 用数组下标代替内存地址";
        l.tutorial_html = make_tut(
            "<strong>静态链表是 PAT 甲级必考招牌题型：</strong>题目给定每个节点的 5 位数字“内存地址”（如 <code>00001</code>）、<code>data</code> 和下一节点地址 <code>next</code>。直接用 <code>Node nodes[100005]</code>（地址即下标）并套用《算法笔记》五步法即可零失误 AC！",
            {
                {"静态链表五步解题套路", "① 定义 <code>struct Node { int addr, data, next, flag; } nodes[100005];</code>（<code>flag</code> 默认初始化 <code>0</code>）；<br>② 读入节点同时填好 <code>addr/data/next</code>；<br>③ 从 <code>head</code> 起沿 <code>next</code> 遍历整条链，标记有效节点 <code>flag = 1</code> 并统计真实长度 <code>cnt</code>；<br>④ <code>sort(nodes, nodes + 100005, cmp)</code> 把有效节点排在最前面；<br>⑤ 遍历 <code>0..cnt-1</code> 重新连接或输出！"},
                {"跳表 (SkipList) 拓展概念", "当需要支持有序序列的快速增删查且嫌红黑树/AVL 实现太重时，可在有序单链表上方每隔 2 节点拉一层稀疏索引（随机化升层），期望查找/插入均为 <code>O(log n)</code>。"}
            }, "",
            "题目输入中常故意塞进根本不在主链表上的<strong>无效孤立废点</strong>（以及空链表 <code>head == -1</code> 或 <code>cnt == 0</code> 的边界），必须先从 <code>head</code> 扫一遍打标记剔除废点后再排序！"
        );
        l.code_template =
R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;

const int MAXN = 100005;
struct Node {
    int addr, data, next;
    int flag; // 1 表示有效节点，0 表示废点
} nodes[MAXN];

bool cmp(const Node& a, const Node& b) {
    if (a.flag != b.flag) return a.flag > b.flag; // 有效节点在前
    return a.data < b.data;
}

int main() {
    for (int i = 0; i < MAXN; ++i) nodes[i].flag = 0;
    int n, head;
    if (scanf("%d %d", &n, &head) != 2) return 0;
    for (int i = 0; i < n; ++i) {
        int a, d, nx; scanf("%d %d %d", &a, &d, &nx);
        nodes[a] = {a, d, nx, 0};
    }
    int cnt = 0;
    for (int p = head; p != -1; p = nodes[p].next) {
        nodes[p].flag = 1;
        ++cnt;
    }
    if (cnt == 0) { printf("0 -1\n"); return 0; }
    sort(nodes, nodes + MAXN, cmp);
    printf("%d %05d\n", cnt, nodes[0].addr);
    for (int i = 0; i < cnt; ++i) {
        if (i + 1 < cnt) printf("%05d %d %05d\n", nodes[i].addr, nodes[i].data, nodes[i + 1].addr);
        else printf("%05d %d -1\n", nodes[i].addr, nodes[i].data);
    }
    return 0;
})CPP";
        add_ex(l, "ex_7_3b", "练习：静态链表过滤废点并按节点值升序重排 (PAT A1052)", "completion",
            "第一行输入 n 与首地址 head，随后 n 行每行 addr data next（可能含不在链表内的无效节点）。过滤无效节点后按 data 升序重排链表，先输出有效长度和排序后新首地址（5位前导零格式，若无有效节点输出 0 -1），再依次输出每个节点 addr data next_addr（末节点 next 为 -1）。",
            "5 00001\n11111 100 -1\n00001 0 22222\n33333 100000 11111\n12345 -1 33333\n22222 1000 12345",
            "5 00001\n00001 0 12345\n12345 -1 11111\n11111 100 22222\n22222 1000 33333\n33333 100000 -1",
            R"JSON([{"label":"5节点重排","input":"4 00001\n00001 30 00002\n00002 10 00003\n00003 20 -1\n99999 -5 00001","expected":"3 00002\n00002 10 00003\n00003 20 00001\n00001 30 -1"},{"label":"全无效空链表","input":"2 -1\n00001 5 -1\n00002 8 00001","expected":"0 -1"}])JSON",
            R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;

const int MAXN = 100005;
struct Node { int addr, data, next, flag; } nodes[MAXN];

int main() {
    for (int i = 0; i < MAXN; ++i) nodes[i].flag = 0;
    int n, head; scanf("%d %d", &n, &head);
    for (int i = 0; i < n; ++i) {
        int a, d, nx; scanf("%d %d %d", &a, &d, &nx);
        nodes[a] = {a, d, nx, 0};
    }
    // TODO: 从 head 遍历打标记、特判 cnt==0、sort 重排并按 %05d 格式输出
    return 0;
})CPP",
            R"CPP(#include <stdio.h>
#include <algorithm>
using namespace std;

const int MAXN = 100005;
struct Node { int addr, data, next, flag; } nodes[MAXN];

bool cmp(const Node& a, const Node& b) {
    if (a.flag != b.flag) return a.flag > b.flag;
    return a.data < b.data;
}

int main() {
    for (int i = 0; i < MAXN; ++i) nodes[i].flag = 0;
    int n, head;
    if (scanf("%d %d", &n, &head) != 2) return 0;
    for (int i = 0; i < n; ++i) {
        int a, d, nx; scanf("%d %d %d", &a, &d, &nx);
        nodes[a] = {a, d, nx, 0};
    }
    int cnt = 0;
    for (int p = head; p != -1; p = nodes[p].next) {
        nodes[p].flag = 1;
        ++cnt;
    }
    if (cnt == 0) { printf("0 -1\n"); return 0; }
    sort(nodes, nodes + MAXN, cmp);
    printf("%d %05d\n", cnt, nodes[0].addr);
    for (int i = 0; i < cnt; ++i) {
        if (i + 1 < cnt) printf("%05d %d %05d\n", nodes[i].addr, nodes[i].data, nodes[i + 1].addr);
        else printf("%05d %d -1\n", nodes[i].addr, nodes[i].data);
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 8 章｜提高篇（2）——搜索专题：DFS 与 BFS（《算法笔记》Ch8 全节）
    // =========================================================================
    chapters.push_back({"ch8", 8, "数据结构与搜索(Ch7-9)", "第 8 章｜搜索专题：深度优先搜索 DFS 与广度优先搜索 BFS", "《算法笔记》Ch8 · 8.1-8.2 原书全节"});

    {
        DsLesson l; l.id = "8.1"; l.chapter_id = "ch8"; l.sort_order = 1;
        l.title = "8.1 深度优先搜索 (DFS)：岔路口递归、选数与剪枝回溯";
        l.source_ref = "《算法笔记》8.1"; l.time_comp = "O(状态数)"; l.space_comp = "O(深度)";
        l.viz_type = "tree"; l.viz_label = "DFS 搜索决策树：选或不选的递归岔路口与剪枝返回";
        l.tutorial_html = make_tut(
            "<strong>《算法笔记》把 DFS 形象比作走迷宫遇到岔路口一路向深处摸索，撞南墙再回溯。</strong>对背包/选数类题目，每个下标 <code>idx</code> 只有两个分支：<strong>选它</strong> 或 <strong>不选它</strong>。",
            {
                {"DFS 递归函数标准三段结构", "① <strong>边界条件 (死胡同)</strong>：如 <code>if (sumW &gt; V) return;</code>（剪枝）或 <code>if (idx == n) { 更新最优解; return; }</code>；<br>② <strong>枚举岔路口</strong>：分别尝试“不选第 <code>idx</code> 个”和“选第 <code>idx</code> 个”；<br>③ <strong>现场还原</strong>：若把路径装入全局 <code>temp.push_back(a[idx])</code>，递归返回后务必 <code>temp.pop_back()</code>！"},
                {"N 皇后对称棋盘剪枝技巧", "逐行 <code>row = 0..n-1</code> 放皇后，只需开三个布尔标记 <code>col[c]</code>（列）、<code>diag1[row-c+n]</code>（主对角线）、<code>diag2[row+c]</code>（副对角线），实现 <code>O(1)</code> 冲突判断！"}
            }, "",
            "若题目允许同一数字重复选择多次，选完 <code>idx</code> 后下一步仍传 <code>dfs(idx, ...)</code>；若每个元素只能选一次，必须传 <code>dfs(idx + 1, ...)</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
using namespace std;

int n = 4, K = 2, X = 6, maxSumSq = -1;
int A[4] = {2, 3, 3, 4};
vector<int> temp, ans;

void dfs(int idx, int nowK, int sum, int sumSq) {
    if (nowK == K && sum == X) {
        if (sumSq > maxSumSq) { maxSumSq = sumSq; ans = temp; }
        return;
    }
    if (idx == n || nowK > K || sum > X) return; // 剪枝
    temp.push_back(A[idx]);
    dfs(idx + 1, nowK + 1, sum + A[idx], sumSq + A[idx] * A[idx]);
    temp.pop_back(); // 恢复现场
    dfs(idx + 1, nowK, sum, sumSq);
}

int main() {
    dfs(0, 0, 0, 0);
    for (int x : ans) cout << x << " ";
    cout << "(sq=" << maxSumSq << ")\n";
    return 0;
})CPP";
        add_ex(l, "ex_8_1", "练习：DFS 回溯求解 N 皇后总方案数", "completion",
            "输入棋盘边长 n (1 <= n <= 11)，补全 dfs 函数，计算在 n x n 棋盘上放置 n 个互不攻击的皇后的合法摆放方案总数。",
            "8", "92",
            R"JSON([{"label":"经典八皇后","input":"8","expected":"92"},{"label":"四皇后","input":"4","expected":"2"},{"label":"1 皇后","input":"1","expected":"1"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

int n, ans = 0;
vector<bool> col, d1, d2;

void dfs(int r) {
    if (r == n) { ++ans; return; }
    // TODO: 枚举列 c，利用 col/d1/d2 O(1) 判冲突并置位/递归/恢复现场
}

int main() {
    cin >> n;
    col.assign(n, false);
    d1.assign(2 * n + 2, false);
    d2.assign(2 * n + 2, false);
    dfs(0);
    cout << ans << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

int n, ans = 0;
vector<bool> col, d1, d2;

void dfs(int r) {
    if (r == n) { ++ans; return; }
    for (int c = 0; c < n; ++c) {
        if (col[c] || d1[r - c + n] || d2[r + c]) continue;
        col[c] = d1[r - c + n] = d2[r + c] = true;
        dfs(r + 1);
        col[c] = d1[r - c + n] = d2[r + c] = false;
    }
}

int main() {
    cin >> n;
    col.assign(n, false);
    d1.assign(2 * n + 2, false);
    d2.assign(2 * n + 2, false);
    dfs(0);
    cout << ans << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "8.2"; l.chapter_id = "ch8"; l.sort_order = 2;
        l.title = "8.2 广度优先搜索 (BFS)：矩阵连通块计数与迷宫最短步数";
        l.source_ref = "《算法笔记》8.2"; l.time_comp = "O(R*C)"; l.space_comp = "O(R*C)";
        l.viz_type = "queue"; l.viz_label = "BFS 水波纹按层扩散：首次到达终点即最短路径";
        l.tutorial_html = make_tut(
            "<strong>在无权图（网格）中求“最少步数/最短距离”，唯一首选就是 BFS：</strong>BFS 用队列逐层向外扩散，因此某点<strong>第一次被入队扩展到时的步数必定是全局最小步数</strong>！",
            {
                {"BFS 标准网格模版", "① 定义四方向偏移数组 <code>dx[4] = {-1, 1, 0, 0}; dy[4] = {0, 0, -1, 1};</code>；<br>② 起点入队，并<strong>立刻在入队这一刻</strong>设置 <code>inq[sx][sy] = true;</code>；<br>③ 每次 <code>auto cur = q.front(); q.pop();</code>，枚举合法未访问邻居入队并将 <code>step</code> 置为 <code>cur.step + 1</code>。"}
            }, "",
            "新手 90% 的 TLE/MLE 都出自<strong>“出队时才标记 vis，而不是入队时就标记 vis”</strong>！若不在入队瞬间标记，同一格子会被多个相邻格子重复塞进队列几十次，直接爆队列或超时！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <queue>
#include <vector>
#include <string>
using namespace std;

int main() {
    vector<string> g = {
        "10010",
        "11011",
        "00000",
        "01100"
    };
    int R = g.size(), C = g[0].size(), blocks = 0;
    vector<vector<bool>> inq(R, vector<bool>(C, false));
    int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
    for (int i = 0; i < R; ++i) {
        for (int j = 0; j < C; ++j) {
            if (g[i][j] == '1' && !inq[i][j]) {
                ++blocks;
                queue<pair<int, int>> q;
                q.push({i, j});
                inq[i][j] = true; // 入队即标记
                while (!q.empty()) {
                    pair<int, int> cur = q.front(); q.pop();
                    int x = cur.first, y = cur.second;
                    for (int k = 0; k < 4; ++k) {
                        int nx = x + dx[k], ny = y + dy[k];
                        if (nx >= 0 && nx < R && ny >= 0 && ny < C && g[nx][ny] == '1' && !inq[nx][ny]) {
                            inq[nx][ny] = true;
                            q.push({nx, ny});
                        }
                    }
                }
            }
        }
    }
    cout << "blocks = " << blocks << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_8_2", "练习：BFS 求网格迷宫从左上角到右下角最短步数", "completion",
            "输入行 R、列 C 及 R 行字符矩阵（'.' 可走，'#' 障碍）。求从 (0,0) 到 (R-1,C-1) 上下左右移动的最少步数，不可达输出 -1。",
            "3 3\n...\n.#.\n...", "4",
            R"JSON([{"label":"3x3 有障碍迷宫","input":"3 3\n...\n.#.\n...","expected":"4"},{"label":"死路不可达","input":"2 2\n.#\n#.","expected":"-1"}])JSON",
            R"CPP(#include <iostream>
#include <queue>
#include <vector>
#include <string>
using namespace std;

int main() {
    int R, C; cin >> R >> C;
    vector<string> g(R);
    for (int i = 0; i < R; ++i) cin >> g[i];
    // TODO: 编写 BFS（入队即标记 dist/vis），输出到 (R-1, C-1) 最短步数或 -1
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <queue>
#include <vector>
#include <string>
using namespace std;

int main() {
    int R, C; cin >> R >> C;
    vector<string> g(R);
    for (int i = 0; i < R; ++i) cin >> g[i];
    if (g[0][0] == '#') { cout << -1 << "\n"; return 0; }
    vector<vector<int>> dist(R, vector<int>(C, -1));
    queue<pair<int, int>> q;
    dist[0][0] = 0;
    q.push({0, 0});
    int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
    while (!q.empty()) {
        pair<int, int> cur = q.front(); q.pop();
        int x = cur.first, y = cur.second;
        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx >= 0 && nx < R && ny >= 0 && ny < C && g[nx][ny] != '#' && dist[nx][ny] == -1) {
                dist[nx][ny] = dist[x][y] + 1;
                q.push({nx, ny});
            }
        }
    }
    cout << dist[R - 1][C - 1] << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 9 章｜提高篇（3）——树与二叉树专题（《算法笔记》Ch9 全 8 节）
    // =========================================================================
    chapters.push_back({"ch9", 9, "数据结构与搜索(Ch7-9)", "第 9 章｜树、二叉树、BST、AVL、并查集、堆与哈夫曼树（9.1-9.8）", "《算法笔记》Ch9 · 9.1-9.8 原书全 8 节"});

    {
        DsLesson l; l.id = "9.1-9.2"; l.chapter_id = "ch9"; l.sort_order = 1;
        l.title = "9.1-9.2 二叉树的存储、四种遍历与已知前/中序还原建树";
        l.source_ref = "《算法笔记》9.1-9.2"; l.time_comp = "O(n)"; l.space_comp = "O(n)";
        l.viz_type = "tree"; l.viz_label = "二叉树根-左-右先序建树与后序/层序遍历";
        l.tutorial_html = make_tut(
            "<strong>二叉树遍历与还原是 PAT 甲级每年必考基础功：</strong>① <strong>先序（根左右）、中序（左根右）、后序（左右根）</strong>递归代码天然只有 4 行；② <strong>层序遍历</strong>借助 <code>queue&lt;Node*&gt;</code> 按层出队即可。",
            {
                {"先序 + 中序 还原建树", "先序第一个元素 <code>pre[preL]</code> 必是当前子树根节点！在中序区间 <code>[inL, inR]</code> 里找到它的下标 <code>k</code>，算出左子树节点个数 <code>numLeft = k - inL</code>，即可递归建出左/右子树！"},
                {"后序 + 中序 还原建树", "同理，后序<strong>最后一个</strong>元素 <code>post[postR]</code> 是根节点，在中序中定位 <code>k</code> 后用 <code>numLeft = k - inL</code> 切分！"},
                {"静态数组二叉树写法", "不想手写指针 <code>new</code> 时可直接用 <code>struct Node { int data, lchild, rchild; } tree[MAXN];</code>（用下标 <code>-1</code> 代替 <code>nullptr</code>）。"}
            }, "",
            "只有“先序+中序”或“后序+中序”（或层序+中序）才能唯一还原二叉树；仅凭“先序+后序”在存在单子节点时无法区分左斜还是右斜（不唯一）！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <queue>
using namespace std;

struct Node {
    int val; Node *l, *r;
    explicit Node(int v) : val(v), l(nullptr), r(nullptr) {}
};

Node* build(const vector<int>& pre, const vector<int>& in, int preL, int preR, int inL, int inR) {
    if (preL > preR) return nullptr;
    Node* root = new Node(pre[preL]);
    int k = inL;
    while (k <= inR && in[k] != pre[preL]) ++k;
    int numLeft = k - inL;
    root->l = build(pre, in, preL + 1, preL + numLeft, inL, k - 1);
    root->r = build(pre, in, preL + numLeft + 1, preR, k + 1, inR);
    return root;
}

void postOrder(Node* p) {
    if (!p) return;
    postOrder(p->l); postOrder(p->r);
    cout << p->val << " ";
}

int main() {
    vector<int> pre = {1, 2, 4, 5, 3}, in = {4, 2, 5, 1, 3};
    Node* root = build(pre, in, 0, 4, 0, 4);
    postOrder(root); cout << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_9_2", "练习：已知二叉树先序与中序遍历求后序遍历", "completion",
            "输入节点数 n，随后输入先序序列与中序序列（互异整数）。构建二叉树后输出其后序遍历序列（空格分隔）。",
            "5\n1 2 4 5 3\n4 2 5 1 3", "4 5 2 3 1",
            R"JSON([{"label":"5 节点树","input":"5\n1 2 4 5 3\n4 2 5 1 3","expected":"4 5 2 3 1"},{"label":"单节点","input":"1\n7\n7","expected":"7"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

vector<int> pre, inSeq, post;

void solve(int preL, int preR, int inL, int inR) {
    if (preL > preR) return;
    // TODO: 切分左右子树并在递归后把根节点 pre[preL] 放入 post
}

int main() {
    int n; cin >> n;
    pre.resize(n); inSeq.resize(n);
    for (int i = 0; i < n; ++i) cin >> pre[i];
    for (int i = 0; i < n; ++i) cin >> inSeq[i];
    solve(0, n - 1, 0, n - 1);
    for (int i = 0; i < n; ++i) cout << post[i] << (i + 1 == n ? '\n' : ' ');
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

vector<int> pre, inSeq, post;

void solve(int preL, int preR, int inL, int inR) {
    if (preL > preR) return;
    int k = inL;
    while (k <= inR && inSeq[k] != pre[preL]) ++k;
    int numLeft = k - inL;
    solve(preL + 1, preL + numLeft, inL, k - 1);
    solve(preL + numLeft + 1, preR, k + 1, inR);
    post.push_back(pre[preL]);
}

int main() {
    int n; cin >> n;
    pre.resize(n); inSeq.resize(n);
    for (int i = 0; i < n; ++i) cin >> pre[i];
    for (int i = 0; i < n; ++i) cin >> inSeq[i];
    solve(0, n - 1, 0, n - 1);
    for (int i = 0; i < n; ++i) cout << post[i] << (i + 1 == n ? '\n' : ' ');
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "9.3"; l.chapter_id = "ch9"; l.sort_order = 2;
        l.title = "9.3 树的静态写法与一般树的遍历 (PAT 多叉树路径与层数)";
        l.source_ref = "《算法笔记》9.3"; l.time_comp = "O(n)"; l.space_comp = "O(n)";
        l.viz_type = "tree"; l.viz_label = "静态数组 Node[MAXN] + vector<int> child 存任意多叉树";
        l.tutorial_html = make_tut(
            "<strong>遇到节点超过 2 个孩子的一般树（多叉树），《算法笔记》统一用静态数组实现：</strong><code>struct Node { int data; vector&lt;int&gt; child; } nodes[MAXN];</code>（若无点权直接用邻接表 <code>vector&lt;int&gt; child[MAXN]</code> 即可）。",
            {
                {"一般树的先根遍历 (DFS)", "从根节点开始遍历 <code>for (int c : child[u]) dfs(c, depth + 1);</code>；若需记录路径权值，把 <code>sum + weight[c]</code> 和路径压入回溯即可。"},
                {"一般树的层序遍历 (BFS)", "在节点上直接加一个 <code>layer</code> 字段（或入队时带深度），统计每一层的节点个数 <code>cnt[layer]++</code>，轻松秒杀家谱最大代数人数等考题！"}
            }, "",
            "题目给出的多叉树中，“某节点的孩子们”并不保证按题目要求的顺序给出；例如 PAT A1053 要求按路径权值从大到小输出时，建树后务必对每个节点的 <code>child</code> 按子节点点权先 <code>sort</code> 一次！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
using namespace std;

vector<vector<int>> child(6);
int layerCnt[10] = {0};

void DFS(int u, int dep) {
    layerCnt[dep]++;
    for (int i = 0; i < static_cast<int>(child[u].size()); ++i) {
        DFS(child[u][i], dep + 1);
    }
}

int main() {
    child[1] = {2, 3};
    child[2] = {4, 5};
    DFS(1, 1);
    for (int d = 1; layerCnt[d] > 0; ++d) {
        cout << "Layer " << d << ": " << layerCnt[d] << " nodes\n";
    }
    return 0;
})CPP";
        add_ex(l, "ex_9_3", "练习：统计家谱多叉树人口最多的一代 (PAT A1094)", "completion",
            "共 n 个成员（编号 1..n，1 号为祖先），其中 m 个成员有子女。随后 m 行每行给出 `id k c1..ck`（节点 id 及其 k 个子女）。输出人口最多一代的人数与该代层号（根节点为第 1 代；同最多时取最小层号）。",
            "6 2\n1 3 2 3 4\n2 2 5 6", "3 2",
            R"JSON([{"label":"3层家谱","input":"6 2\n1 3 2 3 4\n2 2 5 6","expected":"3 2"},{"label":"仅根节点","input":"1 0","expected":"1 1"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

int main() {
    int n, m; cin >> n >> m;
    vector<vector<int>> child(n + 1);
    for (int i = 0; i < m; ++i) {
        int id, k; cin >> id >> k;
        for (int j = 0; j < k; ++j) {
            int c; cin >> c; child[id].push_back(c);
        }
    }
    // TODO: 从根 1 DFS 或 BFS 统计每层人数，输出“最多人数 对应层号”
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

vector<vector<int>> child;
vector<int> cnt;

void DFS(int u, int d) {
    cnt[d]++;
    for (int i = 0; i < static_cast<int>(child[u].size()); ++i) {
        DFS(child[u][i], d + 1);
    }
}

int main() {
    int n, m; cin >> n >> m;
    child.assign(n + 1, {});
    cnt.assign(n + 2, 0);
    for (int i = 0; i < m; ++i) {
        int id, k; cin >> id >> k;
        for (int j = 0; j < k; ++j) {
            int c; cin >> c; child[id].push_back(c);
        }
    }
    DFS(1, 1);
    int bestCnt = 0, bestLvl = 1;
    for (int d = 1; d <= n; ++d) {
        if (cnt[d] > bestCnt) { bestCnt = cnt[d]; bestLvl = d; }
    }
    cout << bestCnt << " " << bestLvl << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "9.4"; l.chapter_id = "ch9"; l.sort_order = 3;
        l.title = "9.4 二叉查找树 (BST)：查找、插入与前驱/后继删除";
        l.source_ref = "《算法笔记》9.4"; l.time_comp = "O(h)"; l.space_comp = "O(n)";
        l.viz_type = "bst"; l.viz_label = "BST 性质：左子树全 < 根 <= 右子树，中序即有序";
        l.tutorial_html = make_tut(
            "<strong>二叉查找树（BST）的核心性质：</strong>左子树所有节点值 <code>&lt;</code> 根，右子树所有节点值 <code>&gt;=</code> 根；因此对 BST 做一次<strong>中序遍历</strong>必然得到严格不降序序列！",
            {
                {"插入建树 (传引用 Node*& root)", "当遇到空指针 <code>if (!root) { root = new Node(x); return; }</code> 时正好新建节点挂载上去！"},
                {"删除节点三大策略", "找到待删节点 <code>p</code>：① 若 <code>p</code> 为叶直接删；② 只有单侧子树时直接用子节点顶替；③ 若既有左又有右子树，找到 <code>p-&gt;l</code> 中最大值（<strong>前驱</strong> <code>findMax(p-&gt;l)</code>）或 <code>p-&gt;r</code> 中最小值（<strong>后继</strong>）替换 <code>p-&gt;data</code>，再递归删去该替身节点！"}
            }, "",
            "BST 的操作复杂度取决于树高 <code>h</code>：随机数据下是 <code>O(log n)</code>；但如果输入本就是有序数列 <code>1, 2, 3, ..., n</code>，BST 会退化成一条单链导致 <code>O(n)</code> 超时——这就是下一节 AVL 树出现的原因！"
        );
        l.code_template =
R"CPP(#include <iostream>
using namespace std;

struct Node {
    int val; Node *l, *r;
    explicit Node(int v) : val(v), l(nullptr), r(nullptr) {}
};

void insertBST(Node*& root, int v) {
    if (!root) { root = new Node(v); return; }
    if (v < root->val) insertBST(root->l, v);
    else insertBST(root->r, v);
}

Node* findMax(Node* p) { while (p && p->r) p = p->r; return p; }

void removeBST(Node*& root, int v) {
    if (!root) return;
    if (v < root->val) removeBST(root->l, v);
    else if (v > root->val) removeBST(root->r, v);
    else {
        if (!root->l && !root->r) { delete root; root = nullptr; }
        else if (root->l) {
            Node* pre = findMax(root->l);
            root->val = pre->val;
            removeBST(root->l, pre->val);
        } else {
            Node* tmp = root; root = root->r; delete tmp;
        }
    }
}

void inorder(Node* p) { if (!p) return; inorder(p->l); cout << p->val << " "; inorder(p->r); }

int main() {
    Node* root = nullptr;
    for (int x : {5, 3, 7, 2, 4, 8}) insertBST(root, x);
    removeBST(root, 5);
    inorder(root); cout << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_9_4", "练习：构造 BST 并输出先序与中序序列", "completion",
            "输入 n 个互不相同整数，按输入顺序依次插入初始为空的 BST（严格小于放左子树，大于放右子树），第一行输出先序遍历，第二行输出中序遍历。",
            "6\n5 3 7 2 4 8", "5 3 2 4 7 8\n2 3 4 5 7 8",
            R"JSON([{"label":"6 节点 BST","input":"6\n5 3 7 2 4 8","expected":"5 3 2 4 7 8\n2 3 4 5 7 8"},{"label":"退化右斜树","input":"3\n1 2 3","expected":"1 2 3\n1 2 3"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

struct Node {
    int val; Node *l, *r;
    explicit Node(int v) : val(v), l(nullptr), r(nullptr) {}
};

void insertBST(Node*& p, int v) {
    // TODO: 实现 BST 递归插入
}

void preOrder(Node* p, vector<int>& out) { if (!p) return; out.push_back(p->val); preOrder(p->l, out); preOrder(p->r, out); }
void inOrder(Node* p, vector<int>& out)  { if (!p) return; inOrder(p->l, out); out.push_back(p->val); inOrder(p->r, out); }

int main() {
    int n; cin >> n;
    Node* root = nullptr;
    for (int i = 0; i < n; ++i) { int x; cin >> x; insertBST(root, x); }
    vector<int> pre, in;
    preOrder(root, pre); inOrder(root, in);
    for (int i = 0; i < n; ++i) cout << pre[i] << (i + 1 == n ? '\n' : ' ');
    for (int i = 0; i < n; ++i) cout << in[i] << (i + 1 == n ? '\n' : ' ');
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

struct Node {
    int val; Node *l, *r;
    explicit Node(int v) : val(v), l(nullptr), r(nullptr) {}
};

void insertBST(Node*& p, int v) {
    if (!p) { p = new Node(v); return; }
    if (v < p->val) insertBST(p->l, v);
    else insertBST(p->r, v);
}

void preOrder(Node* p, vector<int>& out) { if (!p) return; out.push_back(p->val); preOrder(p->l, out); preOrder(p->r, out); }
void inOrder(Node* p, vector<int>& out)  { if (!p) return; inOrder(p->l, out); out.push_back(p->val); inOrder(p->r, out); }

int main() {
    int n; cin >> n;
    Node* root = nullptr;
    for (int i = 0; i < n; ++i) { int x; cin >> x; insertBST(root, x); }
    vector<int> pre, in;
    preOrder(root, pre); inOrder(root, in);
    for (int i = 0; i < n; ++i) cout << pre[i] << (i + 1 == n ? '\n' : ' ');
    for (int i = 0; i < n; ++i) cout << in[i] << (i + 1 == n ? '\n' : ' ');
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "9.5"; l.chapter_id = "ch9"; l.sort_order = 4;
        l.title = "9.5 平衡二叉树 (AVL)：平衡因子与 LL/RR/LR/RL 四种旋转";
        l.source_ref = "《算法笔记》9.5"; l.time_comp = "O(log n)"; l.space_comp = "O(n)";
        l.viz_type = "bst"; l.viz_label = "AVL 平衡因子 [-1..1] 与 LL/RR/LR/RL 四旋转";
        l.tutorial_html = make_tut(
            "<strong>AVL 树在普通 BST 上给每个节点多维护一个树高 <code>h</code>，确保任意节点左右高度差 <code>BF = h(l) - h(r)</code> 始终落在 <code>[-1, 1]</code>，彻底消除退化风险：</strong>",
            {
                {"4 种失衡类型与对应旋转动作", "<ul><li><strong>LL (BF=2, 左子 BF=1)</strong>：直接对当前节点<strong>右旋</strong>；</li><li><strong>LR (BF=2, 左子 BF=-1)</strong>：先对左子<strong>左旋</strong>变成 LL，再对自己<strong>右旋</strong>；</li><li><strong>RR (BF=-2, 右子 BF=-1)</strong>：直接对当前节点<strong>左旋</strong>；</li><li><strong>RL (BF=-2, 右子 BF=1)</strong>：先对右子<strong>右旋</strong>变成 RR，再对自己<strong>左旋</strong>。</li></ul>"}
            }, "",
            "每次做完左右旋转之后，必须按照<strong>“先更新下层旧根高度，再更新上层新根高度”</strong>的顺序调用 <code>updateHeight</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <algorithm>
using namespace std;

struct Node {
    int val, h; Node *l, *r;
    explicit Node(int v) : val(v), h(1), l(nullptr), r(nullptr) {}
};
int H(Node* p) { return p ? p->h : 0; }
int BF(Node* p) { return H(p->l) - H(p->r); }
void upd(Node* p) { p->h = max(H(p->l), H(p->r)) + 1; }

Node* rotRight(Node* y) { Node* x = y->l; y->l = x->r; x->r = y; upd(y); upd(x); return x; }
Node* rotLeft(Node* x)  { Node* y = x->r; x->r = y->l; y->l = x; upd(x); upd(y); return y; }

Node* insertAVL(Node* p, int v) {
    if (!p) return new Node(v);
    if (v < p->val) {
        p->l = insertAVL(p->l, v); upd(p);
        if (BF(p) == 2) {
            if (BF(p->l) < 0) p->l = rotLeft(p->l); // LR 先转左子
            p = rotRight(p);
        }
    } else {
        p->r = insertAVL(p->r, v); upd(p);
        if (BF(p) == -2) {
            if (BF(p->r) > 0) p->r = rotRight(p->r); // RL 先转右子
            p = rotLeft(p);
        }
    }
    return p;
}

int main() {
    Node* root = nullptr;
    for (int x : {88, 70, 61, 96, 120}) root = insertAVL(root, x);
    cout << root->val << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_9_5", "练习：依次插入序列构造 AVL 树并求根节点 (PAT A1066)", "completion",
            "输入 n 个互不相同整数依次插入初始为空的 AVL 树，输出最终 AVL 树根节点的值。",
            "5\n88 70 61 96 120", "70",
            R"JSON([{"label":"PAT A1066 样例","input":"5\n88 70 61 96 120","expected":"70"},{"label":"LR 双旋","input":"3\n10 5 8","expected":"8"}])JSON",
            R"CPP(#include <iostream>
#include <algorithm>
using namespace std;

struct Node {
    int val, h; Node *l, *r;
    explicit Node(int v) : val(v), h(1), l(nullptr), r(nullptr) {}
};
int H(Node* p) { return p ? p->h : 0; }
int BF(Node* p) { return H(p->l) - H(p->r); }
void upd(Node* p) { p->h = max(H(p->l), H(p->r)) + 1; }

Node* rotRight(Node* y) { Node* x = y->l; y->l = x->r; x->r = y; upd(y); upd(x); return x; }
Node* rotLeft(Node* x)  { Node* y = x->r; x->r = y->l; y->l = x; upd(x); upd(y); return y; }

Node* insertAVL(Node* p, int v) {
    if (!p) return new Node(v);
    // TODO: 插入并按 LL/LR/RR/RL 平衡调整后返回新根
    return p;
}

int main() {
    int n; cin >> n;
    Node* root = nullptr;
    for (int i = 0; i < n; ++i) { int x; cin >> x; root = insertAVL(root, x); }
    cout << (root ? root->val : 0) << "\n";
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <algorithm>
using namespace std;

struct Node {
    int val, h; Node *l, *r;
    explicit Node(int v) : val(v), h(1), l(nullptr), r(nullptr) {}
};
int H(Node* p) { return p ? p->h : 0; }
int BF(Node* p) { return H(p->l) - H(p->r); }
void upd(Node* p) { p->h = max(H(p->l), H(p->r)) + 1; }

Node* rotRight(Node* y) { Node* x = y->l; y->l = x->r; x->r = y; upd(y); upd(x); return x; }
Node* rotLeft(Node* x)  { Node* y = x->r; x->r = y->l; y->l = x; upd(x); upd(y); return y; }

Node* insertAVL(Node* p, int v) {
    if (!p) return new Node(v);
    if (v < p->val) {
        p->l = insertAVL(p->l, v); upd(p);
        if (BF(p) == 2) {
            if (BF(p->l) < 0) p->l = rotLeft(p->l);
            p = rotRight(p);
        }
    } else {
        p->r = insertAVL(p->r, v); upd(p);
        if (BF(p) == -2) {
            if (BF(p->r) > 0) p->r = rotRight(p->r);
            p = rotLeft(p);
        }
    }
    return p;
}

int main() {
    int n; cin >> n;
    Node* root = nullptr;
    for (int i = 0; i < n; ++i) { int x; cin >> x; root = insertAVL(root, x); }
    cout << (root ? root->val : 0) << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "9.6"; l.chapter_id = "ch9"; l.sort_order = 5;
        l.title = "9.6 并查集 (DSU)：findFather、Union 与路径压缩";
        l.source_ref = "《算法笔记》9.6"; l.time_comp = "O(alpha(n))"; l.space_comp = "O(n)";
        l.viz_type = "dsu"; l.viz_label = "并查集路径压缩：findFather 把沿途节点直接挂到总根节点";
        l.tutorial_html = make_tut(
            "<strong>并查集是代码最短（核心只需 6 行）、性价比最高的集合合并数据结构：</strong>判断两个人是否在同一个社交圈、检查无向图有几个连通分量、支撑 Kruskal 最小生成树判环，全都靠它。",
            {
                {"路径压缩核心一行代码", "<code>int findFather(int x) { return father[x] == x ? x : father[x] = findFather(father[x]); }</code><br>在递归归来途中顺便把沿线每个节点直接指向总祖先，单次查询均摊复杂度趋近常数 <code>O(&alpha;(n))</code>！"},
                {"合并 Union 写法", "<code>int faA = findFather(a), faB = findFather(b); if (faA != faB) father[faA] = faB;</code>（可同时把集合总数 <code>cnt--</code>）。"}
            }, "",
            "合并前绝对不能偷懒直接写 <code>father[a] = b</code>！必须先各自调用 <code>findFather(a)</code> 与 <code>findFather(b)</code> 找到老祖宗，让两个老祖宗相连才不会破坏原来整棵树！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <numeric>
using namespace std;

struct DSU {
    vector<int> f;
    explicit DSU(int n) : f(n) { iota(f.begin(), f.end(), 0); }
    int find(int x) { return f[x] == x ? x : f[x] = find(f[x]); }
    bool unite(int a, int b) {
        int fa = find(a), fb = find(b);
        if (fa == fb) return false;
        f[fa] = fb;
        return true;
    }
};

int main() {
    DSU dsu(5);
    dsu.unite(1, 2); dsu.unite(2, 3);
    cout << (dsu.find(1) == dsu.find(3) ? "Same" : "Diff") << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_9_6", "练习：并查集求无向图连通分量个数", "completion",
            "给定 n 个节点（编号 1..n）与 m 条无向边 u v，利用路径压缩并查集计算当前连通分量的总数。",
            "5 3\n1 2\n2 3\n4 5", "2",
            R"JSON([{"label":"两个连通块","input":"5 3\n1 2\n2 3\n4 5","expected":"2"},{"label":"含重复自环边","input":"4 3\n1 1\n1 2\n2 1","expected":"3"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <numeric>
using namespace std;

int main() {
    int n, m; cin >> n >> m;
    vector<int> f(n + 1);
    iota(f.begin(), f.end(), 0);
    // TODO: 写出路径压缩 find 与按边合并逻辑，最后输出连通分量个数
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

vector<int> father;

int findFather(int x) {
    if (father[x] == x) return x;
    return father[x] = findFather(father[x]);
}

int main() {
    int n, m; cin >> n >> m;
    father.resize(n + 1);
    for (int i = 1; i <= n; ++i) father[i] = i;
    int comps = n;
    for (int i = 0; i < m; ++i) {
        int u, v; cin >> u >> v;
        int faU = findFather(u), faV = findFather(v);
        if (faU != faV) { father[faU] = faV; --comps; }
    }
    cout << comps << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "9.7"; l.chapter_id = "ch9"; l.sort_order = 6;
        l.title = "9.7 二叉堆：downAdjust/upAdjust 原地建堆与堆排序";
        l.source_ref = "《算法笔记》9.7"; l.time_comp = "O(n log n)"; l.space_comp = "O(1)";
        l.viz_type = "tree"; l.viz_label = "完全二叉树数组存储：父 i，左子 2i，右子 2i+1";
        l.tutorial_html = make_tut(
            "<strong>堆是一棵完全二叉树，因此直接存进 1-indexed 一维数组无需任何指针！</strong>节点 <code>i</code> 的左儿子是 <code>2*i</code>，右儿子是 <code>2*i+1</code>，最后一个非叶子节点必定是 <code>n / 2</code>。",
            {
                {"downAdjust 向下调整 (O(log n))", "比较当前节点 <code>i</code> 与左右儿子中的较大者 <code>j</code>，若 <code>heap[j] &gt; heap[i]</code> 则交换后继续令 <code>i = j, j = i * 2</code> 下沉！"},
                {"O(n) 原地建堆 & O(n log n) 堆排序", "① 从 <code>i = n / 2</code> <strong>倒序</strong>循环到 <code>1</code> 执行 <code>downAdjust(i, n)</code> 即成大顶堆；<br>② 堆排序只需从 <code>i = n</code> 倒序到 <code>2</code>，每次把堆顶最大值 <code>heap[1]</code> 和末尾 <code>heap[i]</code> 交换，然后对缩短后的前缀 <code>[1..i-1]</code> 调用 <code>downAdjust(1, i - 1)</code>！"}
            }, "",
            "建堆与弹出取极值都用 <code>downAdjust</code>；只有在往堆末尾追加新元素时（如插入堆）才需要把末尾元素顺着 <code>i / 2</code> 向上冒泡调用 <code>upAdjust</code>。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

void downAdjust(vector<int>& h, int low, int high) {
    int i = low, j = i * 2;
    while (j <= high) {
        if (j + 1 <= high && h[j + 1] > h[j]) ++j;
        if (h[j] > h[i]) { swap(h[i], h[j]); i = j; j = i * 2; }
        else break;
    }
}

int main() {
    vector<int> h = {0, 85, 55, 82, 57, 68, 92, 99, 98, 66, 56};
    int n = static_cast<int>(h.size()) - 1;
    for (int i = n / 2; i >= 1; --i) downAdjust(h, i, n); // 建大顶堆
    for (int i = n; i > 1; --i) {
        swap(h[1], h[i]);
        downAdjust(h, 1, i - 1);
    }
    for (int i = 1; i <= n; ++i) cout << h[i] << " ";
    cout << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_9_7", "练习：手写大顶堆建堆与就地堆排序", "completion",
            "输入 n 与 n 个整数（放入 1-indexed 数组 h[1..n]），先通过倒序 downAdjust 建立大顶堆，第一行输出建好后的大顶堆层序数组 h[1..n]，第二行完成原地堆排序并输出升序序列。",
            "6\n4 1 7 3 9 2", "9 4 7 3 1 2\n1 2 3 4 7 9",
            R"JSON([{"label":"建堆+堆排双重输出","input":"6\n4 1 7 3 9 2","expected":"9 4 7 3 1 2\n1 2 3 4 7 9"},{"label":"两元素","input":"2\n1 5","expected":"5 1\n1 5"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

void downAdjust(vector<int>& h, int low, int high) {
    // TODO: 补全大顶堆向下调整
}

int main() {
    int n; cin >> n;
    vector<int> h(n + 1);
    for (int i = 1; i <= n; ++i) cin >> h[i];
    // TODO: 1) n/2..1 建大顶堆并输出 h[1..n]; 2) n..2 交换堆顶与末尾并下调再输出 h[1..n]
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

void downAdjust(vector<int>& h, int low, int high) {
    int i = low, j = i * 2;
    while (j <= high) {
        if (j + 1 <= high && h[j + 1] > h[j]) ++j;
        if (h[j] > h[i]) { swap(h[i], h[j]); i = j; j = i * 2; }
        else break;
    }
}

int main() {
    int n; cin >> n;
    vector<int> h(n + 1);
    for (int i = 1; i <= n; ++i) cin >> h[i];
    for (int i = n / 2; i >= 1; --i) downAdjust(h, i, n);
    for (int i = 1; i <= n; ++i) cout << h[i] << (i == n ? '\n' : ' ');
    for (int i = n; i > 1; --i) {
        swap(h[1], h[i]);
        downAdjust(h, 1, i - 1);
    }
    for (int i = 1; i <= n; ++i) cout << h[i] << (i == n ? '\n' : ' ');
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "9.8"; l.chapter_id = "ch9"; l.sort_order = 7;
        l.title = "9.8 哈夫曼树与哈夫曼编码：最小带权路径长度 WPL";
        l.source_ref = "《算法笔记》9.8"; l.time_comp = "O(n log n)"; l.space_comp = "O(n)";
        l.viz_type = "queue"; l.viz_label = "小根优先队列每次弹出两堆最小权值合并累加 WPL";
        l.tutorial_html = make_tut(
            "<strong>哈夫曼树的构造就是经典的“合并果子”贪心模型：</strong>给 n 个叶子权值，不断挑当前<strong>最小的两个权重 <code>x, y</code> 合并成新父节点 <code>x + y</code> 放回集合</strong>，所有非叶节点的权值之和恰好就等于整棵树的最小带权路径长度 <code>WPL = &Sigma; w<sub>i</sub> &times; depth<sub>i</sub></code>！",
            {
                {"为什么非叶节点权值和等于 WPL？", "叶子节点每次被合并上去，它的重量就被多加了一次——共经过几层合并就被累加了几次，总累加次数恰等于它在哈夫曼树中的深度！"},
                {"C++ 小根堆实现只需 6 行", "<code>priority_queue&lt;long long, vector&lt;long long&gt;, greater&lt;long long&gt;&gt; pq;</code> 每次循环弹出前两小 <code>a, b</code>，把 <code>a + b</code> 累加到 <code>ans</code> 并压回 <code>pq</code>，直到 <code>pq.size() == 1</code>！"}
            }, "",
            "当 n 达到 10<sup>4</sup> 或权值较大时，WPL 很容易超过 2<sup>31</sup>-1，务必全部使用 <code>long long</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <queue>
#include <vector>
#include <functional>
using namespace std;

int main() {
    priority_queue<long long, vector<long long>, greater<long long>> pq;
    for (long long w : {1, 2, 2, 3, 6}) pq.push(w);
    long long wpl = 0;
    while (pq.size() > 1) {
        long long a = pq.top(); pq.pop();
        long long b = pq.top(); pq.pop();
        wpl += a + b;
        pq.push(a + b);
    }
    cout << "min WPL = " << wpl << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_9_8", "练习：合并果子求哈夫曼树最小带权路径长度 WPL", "completion",
            "输入 n (n >= 2) 个果子堆（叶子节点）重量，每次只能合并两堆且体力消耗为两堆重量之和，求合成一棵哈夫曼树的最小总代价 WPL。",
            "5\n1 2 2 3 6", "30",
            R"JSON([{"label":"《算法笔记》5堆样例","input":"5\n1 2 2 3 6","expected":"30"},{"label":"经典 1 2 9","input":"3\n1 2 9","expected":"15"}])JSON",
            R"CPP(#include <iostream>
#include <queue>
#include <vector>
#include <functional>
using namespace std;

int main() {
    int n; cin >> n;
    priority_queue<long long, vector<long long>, greater<long long>> pq;
    for (int i = 0; i < n; ++i) { long long w; cin >> w; pq.push(w); }
    // TODO: 不断弹出两个最小权值合并累加进 WPL
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <queue>
#include <vector>
#include <functional>
using namespace std;

int main() {
    int n; cin >> n;
    priority_queue<long long, vector<long long>, greater<long long>> pq;
    for (int i = 0; i < n; ++i) { long long w; cin >> w; pq.push(w); }
    long long ans = 0;
    while (pq.size() > 1) {
        long long a = pq.top(); pq.pop();
        long long b = pq.top(); pq.pop();
        ans += a + b;
        pq.push(a + b);
    }
    cout << ans << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    // =========================================================================
    // 第 10 章｜提高篇（4）——图算法专题（《算法笔记》Ch10 全 7 节）
    // =========================================================================
    chapters.push_back({"ch10", 10, "图论/DP/高阶(Ch10-13)", "第 10 章｜图存储、图遍历、最短路、最小生成树、拓扑与关键路径（10.1-10.7）", "《算法笔记》Ch10 · 10.1-10.7 原书全 7 节"});

    {
        DsLesson l; l.id = "10.1-10.2"; l.chapter_id = "ch10"; l.sort_order = 1;
        l.title = "10.1-10.2 图的定义与存储：邻接矩阵 vs 邻接表";
        l.source_ref = "《算法笔记》10.1-10.2"; l.time_comp = "O(V^2) vs O(V+E)"; l.space_comp = "O(V+E)";
        l.viz_type = "graph"; l.viz_label = "稠密图用邻接矩阵 G[u][v]，稀疏大图用邻接表 vector<Edge> adj[]";
        l.tutorial_html = make_tut(
            "<strong>在《算法笔记》中，决定选邻接矩阵还是邻接表的唯一判据是顶点数 <code>V</code>：</strong>① 当 <code>V &lt;= 1000</code> 时（几乎所有 PAT 题）直接开二维数组 <code>int G[MAXV][MAXV]</code> 最快最省心；② 当 <code>V &gt; 1000</code>（动辄 <code>10<sup>5</sup></code> 顶点的稀疏图）必定爆内存，必须改用邻接表 <code>vector&lt;Edge&gt; adj[MAXV]</code> 或链式前向星！",
            {
                {"邻接表 (最推荐)", "<code>struct Edge { int v, w; }; vector&lt;vector&lt;Edge&gt;&gt; adj(n);</code> 有向边只需 <code>adj[u].push_back({v, w});</code>；无向边正向反向各 push 一次！"},
                {"无向图 vs 有向图度统计", "无向图中某点度就是 <code>adj[u].size()</code>；有向图中 <code>u -&gt; v</code> 使 <code>outDeg[u]++</code>、<code>inDeg[v]++</code>。"}
            }, "",
            "存在重边时若采用邻接矩阵存权值，务必保留较小的边权：<code>G[u][v] = min(G[u][v], w);</code> 千万不能粗心直接覆盖成 <code>G[u][v] = w</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
using namespace std;

struct Edge { int to, w; };

int main() {
    int n = 4;
    vector<vector<Edge>> adj(n);
    vector<int> inDeg(n, 0), outDeg(n, 0);
    auto addEdge = [&](int u, int v, int w) {
        adj[u].push_back({v, w});
        outDeg[u]++; inDeg[v]++;
    };
    addEdge(0, 1, 5); addEdge(0, 2, 3); addEdge(1, 3, 2);
    for (int i = 0; i < n; ++i) {
        cout << "v=" << i << " out=" << outDeg[i] << " in=" << inDeg[i] << "\n";
    }
    return 0;
})CPP";
        add_ex(l, "ex_10_2", "练习：构建有向图邻接表并统计顶点出入度", "completion",
            "输入顶点数 n（编号 0..n-1）与有向边数 m，随后 m 行每行两个整数 u v 表示有向边 u -> v。输出 n 行，每行给出顶点 i 的出度与入度（如 `i: out in`）。",
            "4 4\n0 1\n0 2\n1 3\n2 3", "0: 2 0\n1: 1 1\n2: 1 1\n3: 0 2",
            R"JSON([{"label":"4点有向图","input":"4 4\n0 1\n0 2\n1 3\n2 3","expected":"0: 2 0\n1: 1 1\n2: 1 1\n3: 0 2"},{"label":"孤立点与自环","input":"2 1\n0 0","expected":"0: 1 1\n1: 0 0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

int main() {
    int n, m; cin >> n >> m;
    vector<vector<int>> adj(n);
    vector<int> inDeg(n, 0), outDeg(n, 0);
    // TODO: 读入 m 条有向边填充邻接表与出入度数组并按 `i: out in` 输出
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
using namespace std;

int main() {
    int n, m; cin >> n >> m;
    vector<vector<int>> adj(n);
    vector<int> inDeg(n, 0), outDeg(n, 0);
    for (int i = 0; i < m; ++i) {
        int u, v; cin >> u >> v;
        adj[u].push_back(v);
        outDeg[u]++; inDeg[v]++;
    }
    for (int i = 0; i < n; ++i) {
        cout << i << ": " << outDeg[i] << " " << inDeg[i] << "\n";
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "10.3"; l.chapter_id = "ch10"; l.sort_order = 2;
        l.title = "10.3 图的遍历：DFS 连通分量与 BFS 限层扩散";
        l.source_ref = "《算法笔记》10.3"; l.time_comp = "O(V + E)"; l.space_comp = "O(V + E)";
        l.viz_type = "graph"; l.viz_label = "图的 DFS / BFS 遍历：vis[] 防止有环图无限死循环";
        l.tutorial_html = make_tut(
            "<strong>树是无环连通图，因此树的遍历只需防回头即可；而一般的图可能包含任意环路与多个不连通分支！</strong>",
            {
                {"图遍历两条铁律", "① 务必维护全局访问标记 <code>vis[u] = true</code> 才能阻止在环上无限转圈；<br>② 图可能不连通，对整个图遍历外层需包一层 <code>for (int u = 0; u &lt; n; ++u) if (!vis[u]) DFS(u);</code>！"},
                {"带层数上限 L 的图 BFS (PAT 微博转发)", "在队列里记录节点当前所处深度 <code>layer</code>，只把满足 <code>nextLayer &lt;= L</code> 的未访问邻居计数并入队，精确控制社交网络 N 度人脉扩散范围。"}
            }, "",
            "与网格 BFS 同理：图 BFS 必须在<strong>准备入队时就把 <code>inq[v] = true</code> 设为已入队</strong>，不要等出队再设！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <queue>
using namespace std;

int bfsLimit(const vector<vector<int>>& adj, int s, int L) {
    vector<bool> inq(adj.size(), false);
    queue<pair<int, int>> q;
    inq[s] = true;
    q.push({s, 0});
    int cnt = 0;
    while (!q.empty()) {
        pair<int, int> cur = q.front(); q.pop();
        int u = cur.first, dep = cur.second;
        if (dep >= L) continue;
        for (int v : adj[u]) {
            if (!inq[v]) {
                inq[v] = true;
                ++cnt;
                q.push({v, dep + 1});
            }
        }
    }
    return cnt;
}

int main() {
    vector<vector<int>> adj(5);
    adj[0] = {1, 2}; adj[1] = {0, 3}; adj[2] = {0}; adj[3] = {1, 4}; adj[4] = {3};
    cout << "Within 2 steps from 0: " << bfsLimit(adj, 0, 2) << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_10_3", "练习：图 BFS 统计 L 步内可达的其他顶点数", "completion",
            "输入顶点数 n (0..n-1)、无向边数 m、起点 s 与最大步数 L。随后 m 行给出无向边 u v，求从 s 出发经过不超过 L 条边能到达的除 s 之外的不同顶点总数。",
            "5 4 0 2\n0 1\n0 2\n1 3\n3 4", "3",
            R"JSON([{"label":"含环2步扩散","input":"5 4 0 2\n0 1\n0 2\n1 3\n3 4","expected":"3"},{"label":"步数 L=0","input":"3 2 0 0\n0 1\n1 2","expected":"0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <queue>
using namespace std;

int main() {
    int n, m, s, L; cin >> n >> m >> s >> L;
    vector<vector<int>> adj(n);
    for (int i = 0; i < m; ++i) {
        int u, v; cin >> u >> v;
        adj[u].push_back(v); adj[v].push_back(u);
    }
    // TODO: 编写带层数上限 L 的图 BFS，统计可达顶点数（不含起点 s）
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <queue>
using namespace std;

int main() {
    int n, m, s, L; cin >> n >> m >> s >> L;
    vector<vector<int>> adj(n);
    for (int i = 0; i < m; ++i) {
        int u, v; cin >> u >> v;
        adj[u].push_back(v); adj[v].push_back(u);
    }
    vector<bool> inq(n, false);
    queue<pair<int, int>> q;
    inq[s] = true; q.push({s, 0});
    int cnt = 0;
    while (!q.empty()) {
        pair<int, int> cur = q.front(); q.pop();
        int u = cur.first, dep = cur.second;
        if (dep >= L) continue;
        for (int v : adj[u]) {
            if (!inq[v]) {
                inq[v] = true;
                ++cnt;
                q.push({v, dep + 1});
            }
        }
    }
    cout << cnt << "\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "10.4A"; l.chapter_id = "ch10"; l.sort_order = 3;
        l.title = "10.4A 最短路 (1)：Dijkstra 算法与多标尺边权/点权松弛";
        l.source_ref = "《算法笔记》10.4.1"; l.time_comp = "O((V+E) log V)"; l.space_comp = "O(V + E)";
        l.viz_type = "graph"; l.viz_label = "Dijkstra 贪心选当前最近未确定顶点 + 小根堆松弛邻居";
        l.tutorial_html = make_tut(
            "<strong>非负权单源最短路的绝对王者：</strong>每次从未确定最短距离集合中选出 <code>d[u]</code> 最小的点 <code>u</code>，用它去松弛所有邻居 <code>v</code>。堆优化版本采用 <code>priority_queue&lt;pair&lt;int,int&gt;&gt;</code> 实现，大图可达 <code>O((V+E) log V)</code>。",
            {
                {"多标尺最短路 (PAT 甲级必考！)", "当最短距离相同时，可能需要比第二标尺（如最少花费 <code>cost[]</code>、最大物资数 <code>w[]</code>）或统计最短路条数 <code>num[]</code>：<br>① 当 <code>d[u] + w &lt; d[v]</code>：全面刷新 <code>d[v]</code>，<code>num[v] = num[u]</code>，并刷第二标尺；<br>② 当 <code>d[u] + w == d[v]</code>：累加 <code>num[v] += num[u]</code>，并按需刷新更优第二标尺！"}
            }, "",
            "Dijkstra 贪心成立的生死前提是<strong>所有边权非负</strong>！一旦图中出现负数边权，已出堆确定的节点距离可能被后来的负权边绕行改小，此时必须换 Bellman-Ford / SPFA！另：堆中弹出旧状态 <code>if (du != d[u]) continue;</code> 不可省！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <queue>
#include <functional>
using namespace std;

const int INF = 1e9;
struct Edge { int to, w; };

int main() {
    int n = 4, s = 0;
    vector<vector<Edge>> adj(n);
    adj[0] = {{1, 4}, {2, 1}}; adj[2] = {{1, 2}, {3, 5}}; adj[1] = {{3, 1}};
    vector<int> d(n, INF), num(n, 0);
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    d[s] = 0; num[s] = 1; pq.push({0, s});
    while (!pq.empty()) {
        pair<int, int> cur = pq.top(); pq.pop();
        int du = cur.first, u = cur.second;
        if (du != d[u]) continue;
        for (const auto& e : adj[u]) {
            if (d[u] + e.w < d[e.to]) {
                d[e.to] = d[u] + e.w;
                num[e.to] = num[u];
                pq.push({d[e.to], e.to});
            } else if (d[u] + e.w == d[e.to]) {
                num[e.to] += num[u];
            }
        }
    }
    cout << "dist[3]=" << d[3] << " ways=" << num[3] << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_10_4a", "练习：堆优化 Dijkstra 求单源最短路", "completion",
            "给定有向带非负权图（n 个顶点 0..n-1，m 条边 u v w）和起点 s，输出 s 到每个顶点的最短距离（不可达输出 -1）。",
            "4 5 0\n0 1 4\n0 2 1\n2 1 2\n1 3 1\n2 3 5", "0 3 1 4",
            R"JSON([{"label":"4点松弛经典图","input":"4 5 0\n0 1 4\n0 2 1\n2 1 2\n1 3 1\n2 3 5","expected":"0 3 1 4"},{"label":"含不可达顶点","input":"3 1 0\n0 1 7","expected":"0 7 -1"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <queue>
#include <functional>
using namespace std;

const int INF = 1e9;
struct Edge { int to, w; };

int main() {
    int n, m, s; cin >> n >> m >> s;
    vector<vector<Edge>> adj(n);
    for (int i = 0; i < m; ++i) { int u, v, w; cin >> u >> v >> w; adj[u].push_back({v, w}); }
    // TODO: 编写小根堆优化 Dijkstra，不可达顶点输出 -1
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <queue>
#include <functional>
using namespace std;

const int INF = 1e9;
struct Edge { int to, w; };

int main() {
    int n, m, s; cin >> n >> m >> s;
    vector<vector<Edge>> adj(n);
    for (int i = 0; i < m; ++i) { int u, v, w; cin >> u >> v >> w; adj[u].push_back({v, w}); }
    vector<int> d(n, INF);
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    d[s] = 0; pq.push({0, s});
    while (!pq.empty()) {
        pair<int, int> cur = pq.top(); pq.pop();
        int du = cur.first, u = cur.second;
        if (du != d[u]) continue;
        for (const auto& e : adj[u]) {
            if (d[u] + e.w < d[e.to]) {
                d[e.to] = d[u] + e.w;
                pq.push({d[e.to], e.to});
            }
        }
    }
    for (int i = 0; i < n; ++i) {
        cout << (d[i] >= INF ? -1 : d[i]) << (i + 1 == n ? '\n' : ' ');
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "10.4B"; l.chapter_id = "ch10"; l.sort_order = 4;
        l.title = "10.4B 最短路 (2)：Bellman-Ford / SPFA 判负环与 Floyd 全源最短路";
        l.source_ref = "《算法笔记》10.4.2-10.4.3"; l.time_comp = "O(VE) / O(V^3)"; l.space_comp = "O(V^2)";
        l.viz_type = "graph"; l.viz_label = "Floyd 三重循环 k-i-j：中转点 k 必须放在最外层！";
        l.tutorial_html = make_tut(
            "<strong>有负权边找 Bellman-Ford/SPFA，全源任意两点距离且 V &lt;= 200 直接上 Floyd 6 行代码：</strong>",
            {
                {"Bellman-Ford 与 SPFA 判负环", "Bellman-Ford 对所有边松弛 <code>V - 1</code> 轮，若第 <code>V</code> 轮还能更新则存在可达负环；队列优化版 SPFA 维护 <code>inq[]</code> 和入队次数 <code>cnt[v]</code>，一旦 <code>cnt[v] &gt;= V</code> 立即判定存在负环！"},
                {"Floyd-Warshall 极简全源最短路", "<code>for (int k = 0; k &lt; n; ++k) for (int i = 0; i &lt; n; ++i) for (int j = 0; j &lt; n; ++j) if (d[i][k] + d[k][j] &lt; d[i][j]) d[i][j] = d[i][k] + d[k][j];</code>"}
            }, "",
            "Floyd 的中转节点循环变量 <code>k</code> <strong>必须放在最外层循环</strong>，放在最内层会导致只能用前几个点中转而错失更优路径！另外初始化注意 <code>d[i][i] = 0</code>，其余设 <code>INF = 1e8</code>（防止两 <code>INF</code> 相加溢出 32 位 int）。"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

const int INF = 1e8;

int main() {
    int n = 3;
    vector<vector<int>> d(n, vector<int>(n, INF));
    for (int i = 0; i < n; ++i) d[i][i] = 0;
    d[0][1] = 2; d[1][2] = -3; d[0][2] = 5;
    for (int k = 0; k < n; ++k)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (d[i][k] < INF && d[k][j] < INF)
                    d[i][j] = min(d[i][j], d[i][k] + d[k][j]);
    cout << "0->2 min dist = " << d[0][2] << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_10_4b", "练习：Floyd-Warshall 求解有向图（可含负权无负环）全源最短路", "completion",
            "输入顶点数 n (0..n-1) 与有向边数 m 及查询数 q。接着 m 行给出 u v w（可为负数，保证无负环且重边取小）。随后 q 行询问 u v，输出 u 到 v 的最短距离（不可达输出 INF）。",
            "3 3 2\n0 1 2\n1 2 -3\n0 2 5\n0 2\n2 0", "-1\nINF",
            R"JSON([{"label":"含负权中转优化","input":"3 3 2\n0 1 2\n1 2 -3\n0 2 5\n0 2\n2 0","expected":"-1\nINF"},{"label":"同点距离为0","input":"2 1 1\n0 1 4\n1 1","expected":"0"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

const int INF = 1e8;

int main() {
    int n, m, q; cin >> n >> m >> q;
    vector<vector<int>> d(n, vector<int>(n, INF));
    for (int i = 0; i < n; ++i) d[i][i] = 0;
    for (int i = 0; i < m; ++i) {
        int u, v, w; cin >> u >> v >> w;
        d[u][v] = min(d[u][v], w);
    }
    // TODO: 编写 Floyd 三重循环（注意 k 在最外层及可达性检查），回答 q 次查询
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;

const int INF = 1e8;

int main() {
    int n, m, q; cin >> n >> m >> q;
    vector<vector<int>> d(n, vector<int>(n, INF));
    for (int i = 0; i < n; ++i) d[i][i] = 0;
    for (int i = 0; i < m; ++i) {
        int u, v, w; cin >> u >> v >> w;
        d[u][v] = min(d[u][v], w);
    }
    for (int k = 0; k < n; ++k)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (d[i][k] < INF / 2 && d[k][j] < INF / 2)
                    d[i][j] = min(d[i][j], d[i][k] + d[k][j]);
    while (q--) {
        int u, v; cin >> u >> v;
        if (d[u][v] >= INF / 2) cout << "INF\n";
        else cout << d[u][v] << "\n";
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "10.5"; l.chapter_id = "ch10"; l.sort_order = 5;
        l.title = "10.5 最小生成树 (MST)：Prim 算法与 Kruskal 边排序+并查集";
        l.source_ref = "《算法笔记》10.5"; l.time_comp = "O(E log E)"; l.space_comp = "O(V + E)";
        l.viz_type = "dsu"; l.viz_label = "Kruskal 按边权升序并入不成环的最小生成树边";
        l.tutorial_html = make_tut(
            "<strong>最小生成树（MST）挑出恰好 <code>n - 1</code> 条边把所有 n 个点连成一棵总边权最小的树：</strong>① 稠密图适用 <strong>Prim</strong>（每次把离已生成树集合最近的点拉进树）；② 稀疏图首选 <strong>Kruskal</strong>（极简：所有边排序 + 并查集判环）。",
            {
                {"Kruskal 三步曲", "① 把所有边按权值 <code>w</code> <strong>升序排列</strong>；② 依次取出边 <code>(u, v, w)</code>，用并查集检查若 <code>find(u) != find(v)</code> 则合并 <code>u, v</code>、累加权值并 <code>++cnt</code>；③ 若结束时 <code>cnt &lt; n - 1</code> 则原图不连通，无法生成最小生成树！"}
            }, "",
            "Kruskal 中当收集够 <code>cnt == n - 1</code> 条边时即可提前 <code>break;</code>；另外当 <code>n == 1</code> 时需要 0 条边即已连通（<code>ans = 0</code>）！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
using namespace std;

struct Edge { int u, v, w; };
vector<int> father;

bool cmpEdge(const Edge& a, const Edge& b) { return a.w < b.w; }

int findFather(int x) {
    if (father[x] == x) return x;
    return father[x] = findFather(father[x]);
}

int main() {
    int n = 4;
    vector<Edge> edges = {{0,1,1}, {0,2,4}, {1,2,2}, {1,3,5}, {2,3,3}};
    sort(edges.begin(), edges.end(), cmpEdge);
    father.resize(n);
    for (int i = 0; i < n; ++i) father[i] = i;
    long long ans = 0; int cnt = 0;
    for (int i = 0; i < static_cast<int>(edges.size()); ++i) {
        int fu = findFather(edges[i].u), fv = findFather(edges[i].v);
        if (fu != fv) { father[fu] = fv; ans += edges[i].w; if (++cnt == n - 1) break; }
    }
    cout << "MST total = " << (cnt == n - 1 ? ans : -1) << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_10_5", "练习：Kruskal 最小生成树总权重", "completion",
            "输入顶点数 n（1..n）与无向边数 m，随后 m 行每行 u v w。用 Kruskal 求最小生成树总权重；若原图不连通无法构成生成树，输出 orz。",
            "4 5\n1 2 2\n1 3 2\n1 4 3\n2 3 4\n3 4 3", "7",
            R"JSON([{"label":"4点连通图","input":"4 5\n1 2 2\n1 3 2\n1 4 3\n2 3 4\n3 4 3","expected":"7"},{"label":"图不连通","input":"4 2\n1 2 1\n3 4 2","expected":"orz"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
using namespace std;

struct Edge { int u, v; long long w; };

int main() {
    int n, m; cin >> n >> m;
    vector<Edge> edges(m);
    for (int i = 0; i < m; ++i) cin >> edges[i].u >> edges[i].v >> edges[i].w;
    // TODO: 边按 w 升序 + 并查集构造 MST，不连通输出 orz
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
using namespace std;

struct Edge { int u, v; long long w; };
vector<int> father;

bool cmpEdge(const Edge& a, const Edge& b) { return a.w < b.w; }

int findFather(int x) {
    if (father[x] == x) return x;
    return father[x] = findFather(father[x]);
}

int main() {
    int n, m; cin >> n >> m;
    vector<Edge> edges(m);
    for (int i = 0; i < m; ++i) cin >> edges[i].u >> edges[i].v >> edges[i].w;
    sort(edges.begin(), edges.end(), cmpEdge);
    father.resize(n + 1);
    for (int i = 1; i <= n; ++i) father[i] = i;
    long long ans = 0; int cnt = 0;
    for (int i = 0; i < m; ++i) {
        int fu = findFather(edges[i].u), fv = findFather(edges[i].v);
        if (fu != fv) { father[fu] = fv; ans += edges[i].w; ++cnt; }
    }
    if (cnt == n - 1) cout << ans << "\n";
    else cout << "orz\n";
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }

    {
        DsLesson l; l.id = "10.6-10.7"; l.chapter_id = "ch10"; l.sort_order = 6;
        l.title = "10.6-10.7 拓扑排序 (Kahn 判环) 与关键路径 (AOE 最早/最晚时间)";
        l.source_ref = "《算法笔记》10.6-10.7"; l.time_comp = "O(V + E)"; l.space_comp = "O(V + E)";
        l.viz_type = "graph"; l.viz_label = "DAG 有向无环图 Kahn 入度清零拓扑序";
        l.tutorial_html = make_tut(
            "<strong>课程先修顺序与项目工期管理（AOE 网）都建立在有向无环图（DAG）之上：</strong>",
            {
                {"Kahn 拓扑排序三步 + 判环", "① 统计每个顶点入度 <code>indeg[i]</code>，把所有 <code>indeg == 0</code> 顶点推入队列（若要字典序最小用小根 <code>priority_queue</code>）；<br>② 每次弹出 <code>u</code> 加入答案并删去其所有出边 <code>--indeg[v]</code>，若减到 0 则推入队列；<br>③ 结束时若弹出顶点数 <code>&lt; n</code>，说明图中必定存在<strong>有向环</strong>！"},
                {"关键路径 (AOE 网)", "按<strong>拓扑序</strong>正推每个事件最早发生时间 <code>ve[v] = max(ve[v], ve[u] + w)</code>；再按<strong>逆拓扑序</strong>倒推最晚发生时间 <code>vl[u] = min(vl[u], vl[v] - w)</code>。满足 <code>ve[u] == vl[v] - w</code> 的活动就是工期零延误余量的关键活动！"}
            }, "",
            "算最晚发生时间 <code>vl[]</code> 时，全局汇点的初始 <code>vl[sink]</code> 必须设成整张图的最大 <code>ve</code>（即工程最短总完工时间），而不能盲目设成局部点的 <code>ve</code>！"
        );
        l.code_template =
R"CPP(#include <iostream>
#include <vector>
#include <queue>
#include <functional>
using namespace std;

int main() {
    int n = 4;
    vector<vector<int>> adj(n);
    vector<int> indeg(n, 0);
    auto add = [&](int u, int v) { adj[u].push_back(v); indeg[v]++; };
    add(0, 1); add(0, 2); add(1, 3); add(2, 3);
    priority_queue<int, vector<int>, greater<int>> pq;
    for (int i = 0; i < n; ++i) if (indeg[i] == 0) pq.push(i);
    vector<int> topo;
    while (!pq.empty()) {
        int u = pq.top(); pq.pop();
        topo.push_back(u);
        for (int v : adj[u]) if (--indeg[v] == 0) pq.push(v);
    }
    for (int x : topo) cout << x << " ";
    cout << "\n";
    return 0;
})CPP";
        add_ex(l, "ex_10_6", "练习：字典序最小的拓扑排序（检测是否有环）", "completion",
            "给定有向图顶点数 n（1..n）与边数 m，随后 m 行给出有向边 u -> v。输出字典序最小的拓扑序列；若图中存在环导致无法排出全部顶点，输出 IMPOSSIBLE。",
            "4 4\n1 3\n1 2\n3 4\n2 4", "1 2 3 4",
            R"JSON([{"label":"小根堆保字典序最小","input":"4 4\n1 3\n1 2\n3 4\n2 4","expected":"1 2 3 4"},{"label":"存在有向环","input":"3 3\n1 2\n2 3\n3 1","expected":"IMPOSSIBLE"}])JSON",
            R"CPP(#include <iostream>
#include <vector>
#include <queue>
#include <functional>
using namespace std;

int main() {
    int n, m; cin >> n >> m;
    vector<vector<int>> adj(n + 1);
    vector<int> indeg(n + 1, 0);
    for (int i = 0; i < m; ++i) {
        int u, v; cin >> u >> v;
        adj[u].push_back(v); indeg[v]++;
    }
    // TODO: 用小根 priority_queue 完成 Kahn 拓扑排序，有环输出 IMPOSSIBLE
    return 0;
})CPP",
            R"CPP(#include <iostream>
#include <vector>
#include <queue>
#include <functional>
using namespace std;

int main() {
    int n, m; cin >> n >> m;
    vector<vector<int>> adj(n + 1);
    vector<int> indeg(n + 1, 0);
    for (int i = 0; i < m; ++i) {
        int u, v; cin >> u >> v;
        adj[u].push_back(v); indeg[v]++;
    }
    priority_queue<int, vector<int>, greater<int>> pq;
    for (int i = 1; i <= n; ++i) if (indeg[i] == 0) pq.push(i);
    vector<int> order;
    while (!pq.empty()) {
        int u = pq.top(); pq.pop();
        order.push_back(u);
        for (int v : adj[u]) if (--indeg[v] == 0) pq.push(v);
    }
    if (static_cast<int>(order.size()) != n) {
        cout << "IMPOSSIBLE\n";
    } else {
        for (int i = 0; i < n; ++i) cout << order[i] << (i + 1 == n ? '\n' : ' ');
    }
    return 0;
})CPP");
        lessons.push_back(std::move(l));
    }
}

} // namespace ds_seed
} // namespace omni
