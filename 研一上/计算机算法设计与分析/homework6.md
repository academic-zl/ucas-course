# Homework 4

> Student Name: ZhangLe(张乐)

> Student ID: 201628013229047

## Question 1

首先整数规划问题显然可以在多项式时间内完成验证。所以他是NP问题。

下面证明$3SAT\le_p Integer Programing$

对于任意的3SAT问题，假设其有$n$个变量，$m$个子句。
对于任意一个子句$C_i$，$\neg x_j$变换为$(1-x_j)$，$x_k$变换为$x_k$，$\mathbf b=1$

如 $x_1 \vee x_2 \vee \neg x_3$变为$x_1+x_2+(1-x_3)\ge 1$
显然，经过这样变换以后整数规划$Ax\ge b$其中$x_i=0\ or\ 1$的解就是原3SAT问题的解。

所以，整数规划问题是NPC问题


## Question 3

首先Half 3SAT问题显然可以在多项式时间内完成验证。所以他是NP问题。

下面证明$3SAT\le_p Half 3SAT$

对于任意的3SAT问题，假设其有$n$个变量，$m$个子句。

那么我们可以构造出一个Half 3SAT问题一共有4m个子句。前m个与原3SAT问题一致，接下来的m个子句恒为真。如$x_1 \vee \neg x_2 \vee x_3$。再接下来的2m个子句，要不全为真，要不全为假。如都设置为$x_1 \vee x_2 \vee x_3$。

下面证明这样构造出来的Half 3SAT的解与3SAT的解一致

- 如果某一种赋值是Half 3SAT的解，那么必然有$2m$个子句为假，现在已知其中有$m$个子句恒真，那么只有最后的$2m$个子句为假，所以前$m$个子句为真，所以Half 3SAT的解是3SAT的解

- 如果某一种赋值是3SAT的解，所以$2m$个子句为真，同样就满足Half 3SAT的条件。

所以，Half 3SAT问题是NPC问题