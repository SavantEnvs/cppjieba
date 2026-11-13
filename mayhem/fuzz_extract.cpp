// Keyword-extraction fuzzer: TF-IDF (KeywordExtractor) and TextRank over arbitrary bytes.
// Input: byte 0 selects the extractor, bytes 1-3 are parameters, the rest is the document.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cppjieba/Jieba.hpp"
#include "cppjieba/TextRankExtractor.hpp"

static cppjieba::Jieba* g_jieba;
static cppjieba::TextRankExtractor* g_textrank;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
  g_jieba = new cppjieba::Jieba();
  g_textrank = new cppjieba::TextRankExtractor(*g_jieba, CPPJIEBA_DICT_DIR "/stop_words.utf8");
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 4) return -1;
  const uint8_t mode = data[0] % 4;
  const size_t top_n = data[1];
  const size_t span = 1 + data[2] % 16;
  const size_t rank_time = 1 + data[3] % 16;
  const std::string doc(reinterpret_cast<const char*>(data) + 4, size - 4);

  switch (mode) {
    case 0: {
      std::vector<std::string> keywords;
      g_jieba->extractor.Extract(doc, keywords, top_n);
      break;
    }
    case 1: {
      std::vector<cppjieba::KeywordExtractor::Word> keywords;
      g_jieba->extractor.Extract(doc, keywords, top_n);
      break;
    }
    case 2: {
      std::vector<std::pair<std::string, double> > keywords;
      g_textrank->Extract(doc, keywords, top_n);
      break;
    }
    case 3: {
      std::vector<cppjieba::TextRankExtractor::Word> keywords;
      g_textrank->Extract(doc, keywords, top_n, span, rank_time);
      break;
    }
  }
  return 0;
}
