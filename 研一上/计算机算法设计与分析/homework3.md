# Homework 3

> Student Name: ZhangLe(张乐)

> Student ID: 201628013229047

## Question 1

### Pseudo-code

```c++
bool Havel_Hakimi(arr,n){  
    for(int i=0; i<n-1; ++i){  
        sort(arr+i,arr+n,greater<int>());  
        if(i+arr[i] >= n) return false;  
        for(int j=i+1; j<=i+arr[i] ; ++j){  
            --arr[j];  
            if(arr[j] < 0) return false;  
        }  
    }  
    if(arr[n-1]!=0) return false;  
    return true;  
}  
```

### Provement

算法正确性依赖于Havel-Hakimi定理。下面对Havel-Hakimi定理进行证明。

我对序列进行排序，使得$d_1 \ge d_2 \ge ... \ge d_n$ ，Havel-Hakimi定理提出如果$D=\{d_1,d_2,...,d_n\}$可简单图，那么$D'=\{d_2-1,d_3-1,...,d_{d_1+1}-1,d_{d_1+2},d_{d_1+3},...,d_n\}$可简单图。我们的算法用到这个定理的逆否命题。

若$D$课简单图，设得到的简单图为$G$。分两种情况考虑：

1.若$G$中存在边$(v_1,v_2),(v_1,v_3)...(v_1,v_{d_1+1})$，则去掉这些变几个得到简单图$G'$，于是$D'$可简单图为$G'$
2.如果存在点$v_i,v_j(i<j)$使得$(v_1,v_i)$不在$G$中。此时因为$d_i>=d_j$，必存在$k$使得$(v_i,v_k)$在$G$中但$(v_j,v_k)$不在$G$中。此时我们可以令$G_t=G-\{(v_i,v_k),(v_1,v_j)\}+\{(v_k,v_j),(v_1,v_i)\}$。而$G_t$对应的$D$和原来相同。经过有限次的调整，我们将回到情况1。


### Complexity

最坏情况下，即序列可简单图，对于$d_i$，对剩下的$d_{i+}$做减法的次数不会超过$i$。算法每次循环问题规模减一，即$T(n)=T(n-1)+n$。所以算法时间复杂度为$O(n^2)$

## Question 4

### Pseudo-code

```

max(A[1...n],B[1...n])
    res<=1
    Sort(A);Sort(B)
    for i=1...n
        res*=pow(A[i],B[i]);
    return res

```

### Provement

设$\pi =\prod_{i=1}^n a_i^{b_j}$
假设$a_1 \ge a_2 \ge ... \ge a_n$，$b_1 \ge b_2 \ge ... \ge b_n$。
如果存在项 $a_i^{b_j}$和$a_j^{b_i}(i\ne j)$，显然$a_i^{b_i}a_j^{b_j}\gt a_i^{b_j}a_j^{b_i}$。如果不存在这项，则可以经过有限次两两交换变换为$\prod_{i=1}^n a_i^{b_i}$，在交换过程中，$\pi$不断增加。

### Complexity

算法分为两步，第一步排序数组，第二步计算累乘。第一步时间复杂度为$O(n\log n)$，第二步为$O(n)$。所以算法综合时间复杂度为$O(n\log n)$

## Question 5

```java
package ucas.algorithm;

import java.io.*;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Vector;

/**
 * Created by zl on 2016/10/27.
 */
public class Huffman {
    public String[] encodingTable = new String[256];

    public Node huffmanTree = null;

    private void buildHuffmanTree(int[] freq) {
        List<Node> list = new LinkedList<>();

        for (int i = 0; i < freq.length; i++) {
            if (freq[i] != 0)
                list.add(new Node((char) i, freq[i]));
        }
        while (list.size() != 1) {
            Collections.sort(list);
            Node n1 = list.get(0);
            Node n2 = list.get(1);
            Node r = n1.freq < n2.freq ? new Node((char) 0, n1.freq + n2.freq, n1, n2) : new Node((char) 0, n1.freq + n2.freq, n2, n1);
            list.add(r);
            list.remove(0);
            list.remove(0);
        }
        this.huffmanTree = list.get(0);

        travel(this.huffmanTree, "");

    }

    private void travel(Node r, String code) {
        if (r.isLeaf()) {
            encodingTable[r.val] = code;
            return;
        }
        travel(r.left, code + 0);
        travel(r.right, code + 1);
    }

    char[] encode(char[] bytes) {

        int[] freq = new int[256];
        for (char b : bytes) {
            freq[b] += 1;
        }
        buildHuffmanTree(freq);

        StringBuffer sb = new StringBuffer();
        for (char b : bytes) {
            sb.append(this.encodingTable[b]);
        }
        return str2bytes(sb.toString());
    }


    char[] decode(char[] bytes) {
        String code = bytes2str(bytes);
        List<Character> result = new LinkedList<Character>();
        Node curr = this.huffmanTree;
        for (byte c : code.getBytes()) {
            if (c == '0') {
                curr = curr.left;
            } else {
                curr = curr.right;
            }
            if (curr.isLeaf()) {
                result.add(curr.val);
                curr = this.huffmanTree;
            }
        }
        char[] re = new char[result.size()];
        int i = 0;
        for (Character c : result) {
            re[i++] = c;
        }
        return re;
    }

    char[] str2bytes(String encode) {
        char[] result = new char[(int) Math.ceil(encode.length() / 8.0)];
        for (int i = 0; i < result.length; i++) {
            for (int j = 0; j < 8; j++) {
                result[i] <<= 1;
                if (i * 8 + j < encode.length())
                    result[i] |= encode.charAt(i * 8 + j) == '1' ? 1 : 0;
            }
        }
        return result;
    }

    String bytes2str(char[] bytes) {
        StringBuffer encode = new StringBuffer();
        char[] result = new char[(int) Math.ceil(encode.length() / 8.0)];
        for (int i = 0; i < bytes.length; i++) {
            for (int j = 7; j >= 0; j--) {
                encode.append(((bytes[i] >> j) & 1) > 0 ? '1' : '0');
            }
        }
        return encode.toString();
    }

    public static void main(String args[]) throws Exception {

        Huffman huffman = new Huffman();

        Vector<Character> vector = new Vector<Character>();
        DataInputStream is =
                new DataInputStream(
                        new BufferedInputStream(
                                new FileInputStream("d:\\\\Aesop_Fables.txt")));
        try {
            while (true) {
                vector.add((char) is.readUnsignedByte());
            }
        } catch (Exception e) {

        }

        char[] origin = new char[vector.size()];
        for (int i = 0; i < origin.length; i++) {
            origin[i] = vector.get(i);
        }

        char code[] = huffman.encode(origin);

        DataOutputStream out = null;
        out = new DataOutputStream(new FileOutputStream("d:\\\\out"));
        for (char c : code) {
            out.write(c);
        }
        out.close();

        char ex[] = huffman.decode(code);
        out = new DataOutputStream(new FileOutputStream("d:\\\\ex.txt"));
        for (char c : ex) {
            out.write(c);
        }
        out.close();

        System.out.println("压缩率：" + code.length / (origin.length + 0.0));
    }
}

class Node implements Comparable<Node> {
    public char val;
    public int freq;
    public Node left = null;
    public Node right = null;

    public Node(char val, int freq) {
        this.val = val;
        this.freq = freq;
    }

    public Node(char val, int freq, Node left, Node right) {
        this.val = val;
        this.freq = freq;
        this.left = left;
        this.right = right;
    }

    public boolean isLeaf() {
        return left == null && right == null;
    }

    @Override
    public int compareTo(Node o) {
        return this.freq - o.freq;
    }
}
```