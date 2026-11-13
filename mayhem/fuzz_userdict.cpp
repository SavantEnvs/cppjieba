// User-dictionary fuzzer: parses fuzzer-controlled user dictionary lines into a fresh, small DictTrie
// and segments the same text with it, so no state survives between inputs. The base dictionary is a
// few dozen words (mayhem/fuzz_userdict/tiny.dict.utf8) to keep trie construction cheap per input.
// Modes 2 and 3 call the public DictTrie::DeleteUserWord. An unpatched upstream bug (Trie::DeleteNode reads
// the erased map node: heap-use-after-free on any delete whose first rune is in the trie) is reachable
// through it and is a finding, not something the harness guards; see mayhem/fuzz_userdict/known-findings/.
// Input: byte 0 selects what to exercise, byte 1 is a parameter, the rest is newline-separated text.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cppjieba/MixSegment.hpp"

static cppjieba::HMMModel* g_model;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  g_model = new cppjieba::HMMModel(CPPJIEBA_DICT_DIR "/hmm_model.utf8");
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2) return -1;
  const uint8_t mode = data[0] % 4;
  const uint8_t param = data[1];
  const std::string text(reinterpret_cast<const char*>(data) + 2, size - 2);

  std::vector<std::string> lines;
  size_t start = 0;
  for (size_t i = 0; i <= text.size(); i++) {
    if (i == text.size() || text[i] == '\n') {
      lines.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }

  cppjieba::DictTrie dict(TINY_DICT);
  switch (mode) {
    case 0: dict.LoadUserDict(lines); break;
    case 1:
      dict.InsertUserWord(lines[0], static_cast<int>(param), lines.back());
      break;
    case 2: dict.DeleteUserWord(lines[0], lines.back()); break;
    case 3:
      dict.LoadUserDict(lines);
      dict.DeleteUserWord(lines[0]);
      break;
  }

  cppjieba::MixSegment seg(&dict, g_model);
  std::vector<std::string> words;
  std::vector<std::pair<std::string, std::string> > tagged;
  seg.Cut(text, words, param & 1);
  seg.Tag(text, tagged);
  dict.Find(lines[0]);
  return 0;
}
