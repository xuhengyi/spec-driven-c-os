from __future__ import annotations

from argparse import ArgumentParser
import json

SHELL_SEQUENCES: dict[str, list[str]] = {
    "ch5": [
        "00hello_world",
        "01store_fault",
        "02power",
        "03priv_inst",
        "04priv_csr",
        "12forktest",
        "13forktree",
        "14forktest2",
        "15matrix",
    ],
    "ch6": [
        "00hello_world",
        "01store_fault",
        "02power",
        "03priv_inst",
        "04priv_csr",
        "12forktest",
        "13forktree",
        "14forktest2",
        "15matrix",
        "filetest_simple",
        "cat_filea",
    ],
    "ch7": [
        "00hello_world",
        "01store_fault",
        "02power",
        "03priv_inst",
        "04priv_csr",
        "12forktest",
        "13forktree",
        "14forktest2",
        "15matrix",
        "filetest_simple",
        "cat_filea",
        "sig_simple",
        "sig_simple2",
        "sig_ctrlc",
        "",
        "sig_tests",
    ],
    "ch8": [
        "00hello_world",
        "01store_fault",
        "02power",
        "03priv_inst",
        "04priv_csr",
        "12forktest",
        "13forktree",
        "14forktest2",
        "15matrix",
        "filetest_simple",
        "cat_filea",
        "sig_simple",
        "sig_simple2",
        "sig_ctrlc",
        "",
        "sig_tests",
        "threads",
        "threads_arg",
        "mpsc_sem",
        "sync_sem",
        "race_adder_mutex_blocking",
        "test_condvar",
    ],
}


def shell_sequence(chapter: str) -> list[str]:
    return list(SHELL_SEQUENCES.get(chapter, []))


def main() -> int:
    parser = ArgumentParser(description="Print fixed shell-driving sequence for a chapter.")
    parser.add_argument("chapter")
    args = parser.parse_args()
    print(json.dumps(shell_sequence(args.chapter), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
