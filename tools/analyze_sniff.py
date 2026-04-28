#!/usr/bin/env python3
"""
Analizzatore log CAN Bus Sniffer per Audi A5 B8.

Legge i file CSV prodotti dal CAN sniffer ESP32 e analizza il traffico:
- Raggruppa messaggi per CAN ID
- Mostra conteggio, frequenza media, range di valori per ogni byte
- Confronta sezioni delimitate da marker per trovare byte che cambiano
- Modalita' interattiva per esplorare singoli CAN ID nel tempo
- Esporta risultati in formato markdown

Uso:
    python analyze_sniff.py log.csv                    # analisi completa
    python analyze_sniff.py log.csv --id 0x381         # dettaglio singolo CAN ID
    python analyze_sniff.py log.csv --export report.md # esporta markdown
    python analyze_sniff.py log.csv --interactive      # modalita' interattiva

Formato CSV atteso (dal CAN sniffer):
    timestamp_ms,ID_hex,DLC,B0,B1,B2,B3,B4,B5,B6,B7

Le righe che iniziano con '#' sono commenti o statistiche e vengono ignorate,
tranne le righe MARKER che delimitano le sezioni.

@since 07/04/26 Mattia Alesi
"""

import csv
import sys
import argparse
from collections import defaultdict, OrderedDict


class CanMessage:
    """Singolo messaggio CAN parsato dal log CSV."""

    def __init__(self, timestamp, can_id, dlc, data):
        self.timestamp = timestamp
        self.can_id = can_id
        self.dlc = dlc
        self.data = data  # lista di 8 interi (0-255)


class Section:
    """Sezione di log delimitata da marker."""

    def __init__(self, name, start_time=0):
        self.name = name
        self.start_time = start_time
        self.messages = []  # lista di CanMessage


def parse_log(filepath):
    """
    Legge il file CSV e restituisce le sezioni con i messaggi parsati.
    Le righe MARKER delimitano le sezioni.
    Righe che iniziano con '#' (tranne MARKER) e righe header vengono ignorate.

    @return lista di Section
    """
    sections = [Section("baseline")]
    marker_count = 0

    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            # Rileva marker
            if "MARKER" in line and line.startswith("#"):
                marker_count += 1
                sections.append(Section(f"marker_{marker_count}"))
                continue

            # Ignora commenti e header
            if line.startswith("#") or line.startswith("timestamp"):
                continue

            # Parsa riga CSV
            parts = line.split(",")
            if len(parts) < 3:
                continue

            try:
                timestamp = int(parts[0].strip())
                can_id_str = parts[1].strip()
                can_id = int(can_id_str, 16) if can_id_str.startswith("0x") else int(can_id_str)
                dlc = int(parts[2].strip())

                data = []
                for i in range(3, min(11, len(parts))):
                    val = parts[i].strip()
                    if val:
                        data.append(int(val, 16))
                    else:
                        data.append(0)
                # Pad a 8 byte
                while len(data) < 8:
                    data.append(0)

                msg = CanMessage(timestamp, can_id, dlc, data)
                sections[-1].messages.append(msg)

                if sections[-1].start_time == 0:
                    sections[-1].start_time = timestamp

            except (ValueError, IndexError):
                continue

    # Rimuovi sezioni vuote
    sections = [s for s in sections if len(s.messages) > 0]
    return sections


def get_all_messages(sections):
    """Restituisce tutti i messaggi di tutte le sezioni in ordine."""
    all_msgs = []
    for s in sections:
        all_msgs.extend(s.messages)
    return all_msgs


def analyze_ids(messages):
    """
    Analizza tutti i messaggi e calcola statistiche per CAN ID.

    @return dict: can_id -> {count, freq_hz, first_ts, last_ts,
                             byte_min[8], byte_max[8], byte_values[8][set]}
    """
    stats = OrderedDict()

    # Raggruppa per ID
    by_id = defaultdict(list)
    for msg in messages:
        by_id[msg.can_id].append(msg)

    # Ordina per ID
    for can_id in sorted(by_id.keys()):
        msgs = by_id[can_id]
        count = len(msgs)
        first_ts = msgs[0].timestamp
        last_ts = msgs[-1].timestamp

        freq_hz = 0
        if count > 1 and last_ts > first_ts:
            freq_hz = (count - 1) * 1000.0 / (last_ts - first_ts)

        byte_min = [255] * 8
        byte_max = [0] * 8
        byte_values = [set() for _ in range(8)]

        for msg in msgs:
            for i in range(8):
                v = msg.data[i]
                byte_min[i] = min(byte_min[i], v)
                byte_max[i] = max(byte_max[i], v)
                byte_values[i].add(v)

        stats[can_id] = {
            "count": count,
            "freq_hz": freq_hz,
            "first_ts": first_ts,
            "last_ts": last_ts,
            "byte_min": byte_min,
            "byte_max": byte_max,
            "byte_values": byte_values,
            "dlc": msgs[0].dlc,
        }

    return stats


def compare_sections(sections):
    """
    Confronta le sezioni per trovare byte che cambiano tra una sezione e l'altra.
    Per ogni CAN ID, calcola il valore piu' frequente (moda) di ogni byte in ogni sezione
    e mostra le differenze.

    @return lista di tuple: (can_id, byte_idx, bit_changes, section_values)
    """
    changes = []

    # Raccogli CAN ID presenti in tutte le sezioni
    all_ids = set()
    for s in sections:
        for msg in s.messages:
            all_ids.add(msg.can_id)

    for can_id in sorted(all_ids):
        # Per ogni sezione, calcola la moda di ogni byte
        section_modes = []
        for s in sections:
            msgs_in_section = [m for m in s.messages if m.can_id == can_id]
            if not msgs_in_section:
                section_modes.append(None)
                continue

            modes = []
            for byte_idx in range(8):
                freq = defaultdict(int)
                for m in msgs_in_section:
                    freq[m.data[byte_idx]] += 1
                mode_val = max(freq, key=freq.get)
                modes.append(mode_val)
            section_modes.append(modes)

        # Confronta ogni sezione con la baseline (prima sezione)
        baseline = section_modes[0]
        if baseline is None:
            continue

        for sec_idx in range(1, len(section_modes)):
            sec_data = section_modes[sec_idx]
            if sec_data is None:
                continue

            for byte_idx in range(8):
                if baseline[byte_idx] != sec_data[byte_idx]:
                    # Trova anche quali bit sono cambiati
                    xor = baseline[byte_idx] ^ sec_data[byte_idx]
                    changed_bits = []
                    for bit in range(8):
                        if xor & (1 << bit):
                            changed_bits.append(bit)

                    changes.append({
                        "can_id": can_id,
                        "byte_idx": byte_idx,
                        "section_name": sections[sec_idx].name,
                        "baseline_val": baseline[byte_idx],
                        "section_val": sec_data[byte_idx],
                        "changed_bits": changed_bits,
                    })

    return changes


def print_overview(stats):
    """Stampa panoramica di tutti i CAN ID trovati."""
    print()
    print("=" * 90)
    print("  PANORAMICA CAN ID")
    print("=" * 90)
    print(f"  {'ID':>7} | {'Count':>8} | {'Freq Hz':>8} | {'DLC':>3} | Byte range (min-max per byte)")
    print("-" * 90)

    for can_id, s in stats.items():
        byte_range = ""
        for i in range(s["dlc"]):
            if s["byte_min"][i] == s["byte_max"][i]:
                byte_range += f" {s['byte_min'][i]:02X}    "
            else:
                byte_range += f" {s['byte_min'][i]:02X}-{s['byte_max'][i]:02X}"
        print(f"  0x{can_id:03X} | {s['count']:>8} | {s['freq_hz']:>7.1f} | {s['dlc']:>3} |{byte_range}")

    print("-" * 90)
    total = sum(s["count"] for s in stats.values())
    print(f"  Totale: {total} messaggi, {len(stats)} CAN ID distinti")
    print()


def print_changes(changes, sections):
    """Stampa i byte che cambiano tra le sezioni."""
    if not changes:
        print("  Nessuna differenza trovata tra le sezioni.")
        print("  (Assicurati di aver inserito marker 'm' durante lo sniffing)")
        return

    print()
    print("=" * 95)
    print("  DIFFERENZE TRA SEZIONI (rispetto alla baseline)")
    print("=" * 95)
    print(f"  {'CAN ID':>7} | {'Byte':>4} | {'Bit':>10} | {'Sezione':>15} | {'Baseline':>8} -> {'Nuovo':>8}")
    print("-" * 95)

    for c in changes:
        bits_str = ",".join(str(b) for b in c["changed_bits"])
        print(
            f"  0x{c['can_id']:03X} | B{c['byte_idx']:>3} | bit {bits_str:>6} | "
            f"{c['section_name']:>15} | 0x{c['baseline_val']:02X}     -> 0x{c['section_val']:02X}"
        )

    print("-" * 95)
    # Riepilogo: CAN ID piu' "interessanti" (con piu' cambiamenti)
    id_change_count = defaultdict(int)
    for c in changes:
        id_change_count[c["can_id"]] += 1

    print()
    print("  CAN ID piu' reattivi (ordinati per numero di cambiamenti):")
    for can_id, count in sorted(id_change_count.items(), key=lambda x: -x[1]):
        print(f"    0x{can_id:03X}: {count} cambiamenti")
    print()


def print_id_detail(messages, target_id):
    """Stampa il dettaglio temporale di tutti i messaggi di un singolo CAN ID."""
    filtered = [m for m in messages if m.can_id == target_id]
    if not filtered:
        print(f"  Nessun messaggio trovato per CAN ID 0x{target_id:03X}")
        return

    print()
    print(f"  DETTAGLIO CAN ID 0x{target_id:03X} — {len(filtered)} messaggi")
    print("=" * 80)
    print(f"  {'Timestamp':>12} | B0   B1   B2   B3   B4   B5   B6   B7")
    print("-" * 80)

    prev_data = None
    for msg in filtered:
        line = f"  {msg.timestamp:>12} |"
        for i in range(8):
            # Evidenzia byte che cambiano rispetto al precedente
            if prev_data is not None and msg.data[i] != prev_data[i]:
                line += f" *{msg.data[i]:02X}*"
            else:
                line += f"  {msg.data[i]:02X} "
        print(line)
        prev_data = msg.data

    print("-" * 80)

    # Riepilogo valori unici per byte
    print()
    print("  Valori unici per byte:")
    for i in range(8):
        values = sorted(set(m.data[i] for m in filtered))
        if len(values) == 1:
            print(f"    B{i}: fisso 0x{values[0]:02X}")
        elif len(values) <= 16:
            vals = " ".join(f"0x{v:02X}" for v in values)
            print(f"    B{i}: {len(values)} valori — {vals}")
        else:
            print(f"    B{i}: {len(values)} valori — range 0x{min(values):02X}-0x{max(values):02X}")
    print()


def export_markdown(stats, changes, sections, filepath):
    """Esporta i risultati dell'analisi in formato markdown."""
    with open(filepath, "w", encoding="utf-8") as f:
        f.write("# Analisi CAN Bus Sniffer — Audi A5 B8\n\n")

        # Panoramica
        f.write("## Panoramica CAN ID\n\n")
        f.write("| CAN ID | Conteggio | Freq (Hz) | DLC | Note |\n")
        f.write("|--------|-----------|-----------|-----|------|\n")
        for can_id, s in stats.items():
            f.write(f"| 0x{can_id:03X} | {s['count']} | {s['freq_hz']:.1f} | {s['dlc']} | |\n")
        f.write(f"\n**Totale:** {sum(s['count'] for s in stats.values())} messaggi, ")
        f.write(f"{len(stats)} CAN ID distinti\n\n")

        # Byte range
        f.write("## Dettaglio byte per CAN ID\n\n")
        for can_id, s in stats.items():
            f.write(f"### 0x{can_id:03X} ({s['freq_hz']:.1f} Hz, DLC={s['dlc']})\n\n")
            f.write("| Byte | Min | Max | Valori unici | Fisso? |\n")
            f.write("|------|-----|-----|-------------|--------|\n")
            for i in range(s["dlc"]):
                n_vals = len(s["byte_values"][i])
                fixed = "Si" if n_vals == 1 else "No"
                vals_str = ""
                if n_vals <= 8:
                    vals_str = ", ".join(f"0x{v:02X}" for v in sorted(s["byte_values"][i]))
                else:
                    vals_str = f"{n_vals} valori"
                f.write(
                    f"| B{i} | 0x{s['byte_min'][i]:02X} | 0x{s['byte_max'][i]:02X} | "
                    f"{vals_str} | {fixed} |\n"
                )
            f.write("\n")

        # Differenze tra sezioni
        if changes:
            f.write("## Differenze tra sezioni\n\n")
            f.write("| CAN ID | Byte | Bit cambiati | Sezione | Baseline | Nuovo |\n")
            f.write("|--------|------|-------------|---------|----------|-------|\n")
            for c in changes:
                bits = ",".join(str(b) for b in c["changed_bits"])
                f.write(
                    f"| 0x{c['can_id']:03X} | B{c['byte_idx']} | {bits} | "
                    f"{c['section_name']} | 0x{c['baseline_val']:02X} | 0x{c['section_val']:02X} |\n"
                )
            f.write("\n")

            # CAN ID piu' reattivi
            f.write("### CAN ID piu' reattivi\n\n")
            id_counts = defaultdict(int)
            for c in changes:
                id_counts[c["can_id"]] += 1
            for can_id, count in sorted(id_counts.items(), key=lambda x: -x[1]):
                f.write(f"- **0x{can_id:03X}**: {count} cambiamenti\n")
            f.write("\n")

    print(f"  Report esportato in: {filepath}")


def interactive_mode(messages, stats):
    """Modalita' interattiva: l'utente sceglie CAN ID da esplorare."""
    print()
    print("  MODALITA' INTERATTIVA")
    print("  Digita un CAN ID in hex (es. 381) per vedere il dettaglio.")
    print("  Digita 'list' per la lista ID, 'quit' per uscire.")
    print()

    while True:
        try:
            user_input = input("  CAN ID> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            break

        if user_input in ("quit", "exit", "q"):
            break
        elif user_input == "list":
            for can_id in stats:
                print(f"    0x{can_id:03X} — {stats[can_id]['count']} msg, {stats[can_id]['freq_hz']:.1f} Hz")
            print()
        else:
            try:
                target = int(user_input.replace("0x", ""), 16)
                print_id_detail(messages, target)
            except ValueError:
                print("  Formato non valido. Usa hex, es: 381 oppure 0x381")


def main():
    parser = argparse.ArgumentParser(
        description="Analizzatore log CAN Bus Sniffer per Audi A5 B8"
    )
    parser.add_argument("logfile", help="File CSV dal CAN sniffer")
    parser.add_argument(
        "--id",
        help="Mostra dettaglio per un singolo CAN ID (hex, es. 0x381)",
    )
    parser.add_argument(
        "--export",
        help="Esporta risultati in file markdown",
    )
    parser.add_argument(
        "--interactive", "-i",
        action="store_true",
        help="Attiva modalita' interattiva",
    )

    args = parser.parse_args()

    # Parsa il log
    print(f"\n  Lettura file: {args.logfile}")
    sections = parse_log(args.logfile)

    if not sections:
        print("  ERRORE: nessun messaggio CAN trovato nel file.")
        print("  Verifica che il formato sia: timestamp_ms,ID_hex,DLC,B0,...,B7")
        sys.exit(1)

    all_messages = get_all_messages(sections)
    print(f"  Trovati {len(all_messages)} messaggi in {len(sections)} sezioni")

    # Analisi per ID
    stats = analyze_ids(all_messages)

    # Dettaglio singolo ID
    if args.id:
        target = int(args.id.replace("0x", ""), 16)
        print_id_detail(all_messages, target)
        return

    # Panoramica
    print_overview(stats)

    # Confronto sezioni (se ci sono marker)
    changes = []
    if len(sections) > 1:
        changes = compare_sections(sections)
        print_changes(changes, sections)
    else:
        print("  Solo 1 sezione trovata (nessun marker).")
        print("  Usa il comando 'm' sullo sniffer per inserire marker tra le azioni.")

    # Export markdown
    if args.export:
        export_markdown(stats, changes, sections, args.export)

    # Modalita' interattiva
    if args.interactive:
        interactive_mode(all_messages, stats)


if __name__ == "__main__":
    main()
