// Segmentation fuzzer: every cut mode of cppjieba::Jieba over arbitrary bytes.
// Input: byte 0 selects the mode, byte 1 is a parameter, the rest is the sentence.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cppjieba/Jieba.hpp"

static cppjieba::Jieba* g_jieba;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  g_jieba = new cppjieba::Jieba();
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2) return -1;
  const uint8_t mode = data[0] % 10;
  const uint8_t param = data[1];
  const std::string sentence(reinterpret_cast<const char*>(data) + 2, size - 2);
  const cppjieba::Jieba& jieba = *g_jieba;

  std::vector<std::string> words;
  std::vector<cppjieba::Word> located;
  std::vector<std::pair<std::string, std::string> > tagged;
  switch (mode) {
    case 0: jieba.Cut(sentence, words, true); break;
    case 1: jieba.Cut(sentence, words, false); break;
    case 2: jieba.CutAll(sentence, words); break;
    case 3: jieba.CutForSearch(sentence, words, true); break;
    case 4: jieba.CutForSearch(sentence, words, false); break;
    case 5: jieba.CutHMM(sentence, words); break;
    case 6: jieba.CutSmall(sentence, words, 1 + param); break;
    case 7: jieba.Tag(sentence, tagged); break;
    case 8: jieba.Cut(sentence, located, param & 1); break;
    case 9: jieba.CutForSearch(sentence, located, param & 1); break;
  }
  return 0;
}
