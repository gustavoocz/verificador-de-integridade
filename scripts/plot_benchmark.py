#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
plot_benchmark.py — Gera gráficos do benchmark do Cache LRU (Tema 14)

Gera um gráfico comparativo de computações de hash (Passe 1 x Passe 2)
e outro com a taxa de acerto (Hit Ratio) no Passe 2, salvando em
scripts/benchmark_results.png.

Tema 14 — Verificador de Integridade de Disco
Trabalho Interdisciplinar ED × SO — Ifes Cachoeiro
"""

import sys
import os
import csv

# Usado para evitar problemas com interfaces gráficas não configuradas
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# Dados reais coletados pelo benchmark (fallback padrão)
fallback_data = [
    # (cache_cap, pass1_hash, pass2_hash, hit_ratio_p2_percent)
    (0, 128, 128, 0.0),
    (8, 640, 640, 12.5),
    (16, 416, 416, 38.89),
    (32, 392, 392, 45.45),
    (64, 384, 384, 48.44),
    (128, 382, 382, 49.61),
    (256, 382, 256, 100.0)
]

def load_data():
    cache_caps = []
    pass1_hashes = {}
    pass2_hashes = {}
    hit_ratios = {}

    # Caso um arquivo CSV tenha sido passado por parâmetro
    if len(sys.argv) > 1:
        csv_path = sys.argv[1]
        if not os.path.exists(csv_path):
            print(f"Erro: Arquivo CSV '{csv_path}' nao encontrado.")
            sys.exit(1)

        print(f"Lendo dados de benchmark a partir de: {csv_path}")
        with open(csv_path, "r", newline="", encoding="utf-8") as f:
            reader = csv.reader(f)
            try:
                header = next(reader)
            except StopIteration:
                print("Erro: Arquivo CSV vazio.")
                sys.exit(1)

            header_normalized = [col.strip().lower() for col in header]

            try:
                pass_idx = next(i for i, col in enumerate(header_normalized) if "pass" in col)
                cache_cap_idx = next(i for i, col in enumerate(header_normalized) if "cache_cap" in col or "cache_capacity" in col)
                hash_comp_idx = next(i for i, col in enumerate(header_normalized) if "hash_comp" in col or "hash_computations" in col)
                hit_ratio_idx = next(i for i, col in enumerate(header_normalized) if "hit_ratio" in col)
            except StopIteration:
                print("Erro: Colunas obrigatorias nao encontradas no CSV.")
                print("O cabeçalho deve conter: 'pass', 'cache_cap', 'hash_comp' e 'hit_ratio'")
                sys.exit(1)

            data_by_cap = {}
            for row in reader:
                if not row or not row[0].strip():
                    continue
                try:
                    p = int(row[pass_idx].strip())
                    cap = int(row[cache_cap_idx].strip())
                    hashes = int(float(row[hash_comp_idx].strip()))
                    ratio = float(row[hit_ratio_idx].strip())

                    # Se a taxa estiver no intervalo 0..1, converte para 0..100%
                    if ratio <= 1.0 and ratio > 0.0:
                        ratio *= 100.0

                    if cap not in data_by_cap:
                        data_by_cap[cap] = {}
                    data_by_cap[cap][p] = hashes
                    if p == 2:
                        data_by_cap[cap]["hit_ratio_p2"] = ratio
                except Exception as e:
                    print(f"Aviso: Ignorando linha mal formatada {row}: {e}")

            sorted_caps = sorted(data_by_cap.keys())
            for cap in sorted_caps:
                cache_caps.append(cap)
                pass1_hashes[cap] = data_by_cap[cap].get(1, 0)
                pass2_hashes[cap] = data_by_cap[cap].get(2, 0)
                hit_ratios[cap] = data_by_cap[cap].get("hit_ratio_p2", 0.0)
    else:
        print("Nenhum CSV fornecido. Usando os dados de benchmark reais embutidos.")
        for cap, p1, p2, ratio in fallback_data:
            cache_caps.append(cap)
            pass1_hashes[cap] = p1
            pass2_hashes[cap] = p2
            hit_ratios[cap] = ratio

    return cache_caps, pass1_hashes, pass2_hashes, hit_ratios

def main():
    cache_caps, pass1_hashes, pass2_hashes, hit_ratios = load_data()

    # Configuração dos índices e rótulos do eixo X (uniformemente espaçados para clareza)
    x_indices = range(len(cache_caps))
    x_labels = [str(cap) for cap in cache_caps]

    # Criação da figura com dois subplots (lado a lado)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 8), dpi=150)
    fig.suptitle("Verificador de Integridade — Benchmark do Cache LRU", fontsize=16, fontweight="bold")

    # --- Gráfico 1: Computações de Hash (Linha) ---
    y_p1 = [pass1_hashes[cap] for cap in cache_caps]
    y_p2 = [pass2_hashes[cap] for cap in cache_caps]

    ax1.plot(x_indices, y_p1, marker="o", linewidth=2, color="#1f77b4", label="Passe 1 (Cache Frio)")
    ax1.plot(x_indices, y_p2, marker="s", linewidth=2, color="#ff7f0e", label="Passe 2 (Cache Quente)")
    ax1.set_xticks(x_indices)
    ax1.set_xticklabels(x_labels)
    ax1.set_xlabel("Capacidade do Cache (Nós)", fontsize=12)
    ax1.set_ylabel("Computações de Hash (SHA-256)", fontsize=12)
    ax1.set_title("Computações de Hash por Passe", fontsize=13, fontweight="bold", pad=10)
    ax1.grid(True, linestyle="--", alpha=0.5)
    ax1.legend(fontsize=10, loc="best")

    # --- Gráfico 2: Taxa de Hit no Passe 2 (Barras) ---
    y_ratio = [hit_ratios[cap] for cap in cache_caps]
    bars = ax2.bar(x_indices, y_ratio, color="#2ca02c", alpha=0.8, edgecolor="black", width=0.6)
    
    # Destaca em vermelho a capacidade 0 (sem hits)
    for idx, bar in enumerate(bars):
        if y_ratio[idx] == 0.0:
            bar.set_color("#d62728")

    ax2.set_xticks(x_indices)
    ax2.set_xticklabels(x_labels)
    ax2.set_xlabel("Capacidade do Cache (Nós)", fontsize=12)
    ax2.set_ylabel("Taxa de Hit (%)", fontsize=12)
    ax2.set_ylim(0, 115)
    ax2.set_title("Taxa de Hit no Passe 2", fontsize=13, fontweight="bold", pad=10)
    ax2.grid(True, linestyle="--", alpha=0.5, axis="y")

    # Adiciona rótulos de valores em cima de cada barra
    for idx, val in enumerate(y_ratio):
        ax2.text(idx, val + 2, f"{val:.1f}%", ha="center", va="bottom", fontsize=9, fontweight="bold")

    plt.tight_layout(rect=[0, 0, 1, 0.94])

    # Salva o gráfico
    os.makedirs("scripts", exist_ok=True)
    png_path = "scripts/benchmark_results.png"
    plt.savefig(png_path, dpi=150)
    plt.close()

    print(f"Grafico salvo em {png_path}")

if __name__ == "__main__":
    main()
