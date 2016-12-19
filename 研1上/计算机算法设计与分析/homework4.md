# Homework 4

> Student Name: ZhangLe(张乐)

> Student ID: 201628013229047

## Question 2

题目是最大化最小两个航班的降落时间间隔，我们不妨两个航班的降落时间最小间隔为$c$。同时设第$i$个飞机的降落时间为$x_i$。

令
$$
\mathbf{x}=
\begin{pmatrix}
x_1 \\
\vdots \\
x_n
\end{pmatrix}
$$
于是有：

$$
Z=\max{c}\\
s.t.
\left\{
\begin{array}{ccc}
\mathbf{x}&\ge&\mathbf{s}\\
\mathbf{x}&\le&\mathbf{t}\\
\begin{pmatrix}
\mathbf{x} \\
x_n
\end{pmatrix}-
\begin{pmatrix}
x_1 \\
\mathbf{x} \\
\end{pmatrix}
&\ge&
\begin{pmatrix}
0 \\
c \\
\vdots \\
c \\
0 \\
\end{pmatrix}
\end{array}
\right.
$$


其中$\mathbf{s}=
\begin{pmatrix}
s_1\\
s_2\\
\vdots\\
s_n\\
\end{pmatrix}
$
，
$\mathbf{t}=
\begin{pmatrix}
t_1\\
t_2\\
\vdots\\
t_n\\
\end{pmatrix}
$

经过这一转换，我们将原问题转化为一个线性规划问题。

这里假设10:00位第0分钟，那么后面的时间可以表示为10:00到当前时间所经过的时间

那么则有：
$
\mathbf{s}=
\begin{pmatrix}
0\\
80\\
120\\
\end{pmatrix}
$
，
$
\mathbf{t}=
\begin{pmatrix}
60\\
100\\
140\\
\end{pmatrix}
$



## Question 4

题目是最小化两个加油站之间的最大距离，我们不妨设两个加油站之间的最大距离为$c$。同时设第$i$个加油站距离第一$x_i$。


令
$$
\mathbf{x}=
\begin{pmatrix}
x_1 \\
\vdots \\
x_n
\end{pmatrix}
$$
于是有：


$$
Z=\max{c}\\
s.t.
\left\{
\begin{array}{ccc}
\mathbf{x}&\ge&
\begin{pmatrix}
d_i-r\\
d_2-r\\
\vdots\\
d_n-r
\end{pmatrix}\\
\mathbf{x}&\le&
\begin{pmatrix}
d_i+r\\
d_2+r\\
\vdots\\
d_n+r
\end{pmatrix}\\
\begin{pmatrix}
\mathbf{x} \\
x_n
\end{pmatrix}-
\begin{pmatrix}
x_1 \\
\mathbf{x} \\
\end{pmatrix}
&\le&
\begin{pmatrix}
0 \\
c \\
\vdots \\
c \\
0 \\
\end{pmatrix}
\end{array}
\right.
$$

原问题转换为了线性规划问题，求解此问题即可完成原问题的求解