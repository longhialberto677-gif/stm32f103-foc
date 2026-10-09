#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 FOC 查表用的正弦表 (sin_tab)，输出可直接粘进 FocAlgorithm.c。

用法:
    python gen_sin_tab.py            # 默认 1024 点表 (4KB Flash)
    python gen_sin_tab.py 256        # 256 点表 (1KB Flash)，参数必须是 2 的幂

表定义: TAB[i] = sin(2*pi*i/TAB_SIZE), i = 0 .. TAB_SIZE-1
索引 0 .. TAB_SIZE/4 即 0 .. pi/2，整表覆盖一个完整电周期。
"""

import sys
import math

TAB_SIZE_DEFAULT = 1024                   # 1024 点 -> 4KB Flash
PER_LINE = 4                              # 每行 4 个数, 方便阅读


def get_tab_size():
    """取表点数；无参数用默认值。做严格校验，防止非法值导致巨大表。"""
    if len(sys.argv) <= 1:
        return TAB_SIZE_DEFAULT

    try:
        size = int(sys.argv[1])
    except ValueError:
        raise SystemExit("参数必须是整数（2 的幂），例如 256 / 1024 / 2048")

    if size < 16 or size > 8192:
        raise SystemExit("表点数必须在 16 ~ 8192 之间，收到: %d" % size)
    if size & (size - 1):
        raise SystemExit("表点数必须是 2 的幂，收到: %d" % size)
    return size


def main():
    size = get_tab_size()
    tab_bits = size.bit_length() - 1       # 1024 -> 10

    print("/* ===== sin_tab 自动生成，请勿手改 =====")
    print(" * TAB[%d] = sin(2*pi*i/%d), i = 0..%d" % (size, size, size - 1))
    print(" * 由 tools/gen_sin_tab.py 生成（%d 点，对应 SIN_TAB_BITS = %d）"
          % (size, tab_bits))
    print(" */")
    print("const float sin_tab[SIN_TAB_SIZE] =")
    print("{")

    for i in range(0, size, PER_LINE):
        vals = []
        for j in range(PER_LINE):
            k = i + j
            if k >= size:
                break
            # 先算弧度再取正弦，round 到 7 位小数，避免出现 1.0000001
            rad = 2.0 * math.pi * float(k) / float(size)
            vals.append("%.7ff" % round(math.sin(rad), 7))
        print("    " + ", ".join(vals) + ",")

    print("};")


if __name__ == "__main__":
    main()
