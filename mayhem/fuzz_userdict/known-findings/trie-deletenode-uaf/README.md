# `Trie::DeleteNode` heap-use-after-free (found by Mayhem run fuzz-userdict #1)

`include/cppjieba/Trie.hpp`, `Trie::DeleteNode(key, ptValue)`:

```cpp
kmIter = ptNode->next->find(*citer);
...
ptNode->next->erase(*citer);   // frees the map node kmIter points at
ptNode = kmIter->second;       // read of the freed node  (Trie.hpp:165)
delete ptNode;
```

Any `DictTrie::DeleteUserWord(word)` whose first rune is a child of the trie root reads freed memory
(ASan: `heap-use-after-free READ of size 8`). It also deletes the whole subtree under that first rune
instead of just the word. Fix: take `ptNode = kmIter->second` before `erase`, and only clear the
`ptValue` (or prune) the node for the full key.

Minimal reproducer (ASan, from the repo root, `-Iinclude`; confirmed):

```cpp
cppjieba::DictTrie d("dict/jieba.dict.utf8");
d.InsertUserWord("北京");
d.DeleteUserWord("北京");   // heap-use-after-free at Trie.hpp:165
```

`crash-7081c415` is the input Mayhem saved from fuzz-userdict run #1 (first byte 0xff, mode 3: load the user
dictionary, then delete line 0). `fuzz_userdict` still calls `DeleteUserWord` in modes 2 and 3, so
`./fuzz_userdict-standalone crash-7081c415` reproduces the ASan report at `Trie.hpp:165`. The file is kept
out of `testsuite/` on purpose: seeds are replayed every run.

Measured against the harness as shipped (local, image build, `-seed=1..100`):

| probe | crashes |
| --- | --- |
| the 15 seeds in `testsuite/`, each replayed once (`-standalone`, `-runs=0`) | 0 / 15 |
| `-runs=5` over a copy of `testsuite/` | 0 / 100 |
| `-runs=100` over a copy of `testsuite/` (85 mutations after the seeds) | 25 / 100 |
| `-runs=5` over an empty corpus | 2 / 100 |
| `-runs=100` over an empty corpus | 90 / 100 |

The seeds never crash. Any fuzzing past them finds the bug within a few dozen executions.
