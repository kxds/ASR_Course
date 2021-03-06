#include <set>
#include "lang_model.H"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

LangModel::LangModel(const map<string, string>& params)
    : m_params(params),
      m_symTable(new SymbolTable(get_required_string_param(m_params, "vocab"))),
      m_bosIdx(m_symTable->get_index(get_string_param(m_params, "bos", "<s>"))),
      m_eosIdx(
          m_symTable->get_index(get_string_param(m_params, "eos", "</s>"))),
      m_unkIdx(
          m_symTable->get_index(get_string_param(m_params, "unk", "<UNK>"))),
      m_n(get_int_param(m_params, "n", 3)) {
  if ((m_bosIdx == -1) || (m_eosIdx == -1) || (m_unkIdx == -1))
    throw runtime_error("Vocabulary missing BOS/EOS/UNK token.");

  //  Get training counts.
  ifstream inStrm(get_required_string_param(params, "train").c_str());
  string lineStr;
  vector<string> wordList;
  vector<int> wordIdxList;
  while (inStrm.peek() != EOF) {
    //  Read line, split into tokens, convert to indices, count n-grams.
    getline(inStrm, lineStr);
    split_string(lineStr, wordList);
    convert_words_to_indices(wordList, wordIdxList, get_sym_table(),
                             get_ngram_length(), get_bos_index(),
                             get_eos_index(), get_unknown_index());
    count_sentence_ngrams(wordIdxList);
  }

  string countFile = get_string_param(params, "count_file");
  if (!countFile.empty()) write_counts(countFile);
}

void LangModel::write_counts(const string& fileName) const {
  ofstream outStrm(fileName.c_str());
  outStrm << "# Pred counts.\n";
  m_predCounts.write(outStrm, get_sym_table());
  outStrm << "# Hist counts.\n";
  m_histCounts.write(outStrm, get_sym_table());
  outStrm << "# Hist 1+ counts.\n";
  m_histOnePlusCounts.write(outStrm, get_sym_table());
  outStrm.close();
}

void LangModel::count_sentence_ngrams(const vector<int>& wordList) {
  //
  //  BEGIN_LAB
  //
  //  This routine is called for each sentence in the training
  //  data.  It should collect all relevant n-gram counts for
  //  the sentence.
  //
  //  Input:
  //      "m_n" = the value of "n" for the n-gram model; e.g.,
  //          this has the value 3 for a trigram model.
  //      "wordCnt" = the number of words in the vector "wordList".
  //
  //      The vector "wordList[0 .. (wordCnt-1)]" holds the sentence
  //      as a sequence of integer indices (each integer
  //      representing a word).  This vector is padded with
  //      beginning-of-sentence and end-of-sentence markers in
  //      a convenient manner for counting.  In particular,
  //      "wordList[0 .. (m_n - 2)]" holds beginning-of-sentence markers, so
  //      the first "real" word of the sentence is "wordList[m_n - 1]";
  //      and "wordList[wordCnt-1]" holds the end-of-sentence marker.
  //
  //  Output:
  //      Update all the counts needed for Witten-Bell smoothing.
  //
  //      Specifically, the objects "m_predCounts", "m_histCounts", and
  //      "m_histOnePlusCounts" are all of type "NGramCounter", a class
  //      that can be used to store counts for a set of n-grams.
  //      The object "m_predCounts" should be used to store
  //      "regular" n-gram counts; the object "m_histCounts" should
  //      be used to store "history" n-gram counts (i.e., the
  //      normalization count for that n-gram occuring as a history);
  //      and "m_histOnePlusCounts" should be used to store the
  //      number of different words following that n-gram history.
  //      To increment the count of a particular n-gram in
  //      one of these objects, you can do a call like:
  //
  //      m_predCounts.incr_count(ngram);
  //
  //      where "ngram" is of type "vector<int>".  This call returns
  //      the value of the incremented count.
  //
  //      Your code should work for any value of m_n (larger than zero).
  vector<int>::const_iterator it, first1, last1, last2;

  for (it = wordList.begin() ; it != wordList.end(); it++ ) {
    vector<int> cut1_vector, cut2_vector;

    for (int i = 0; i <= m_n; i++ ){
      first1 = it;
      last1 = min(it + i, wordList.end());
      cut1_vector.assign(first1, last1);  
      m_predCounts.incr_count(cut1_vector);      
      if (last1 + 1 < wordList.end()) {
        m_histCounts.incr_count(cut1_vector);
        if ( (i>0) && (m_predCounts.get_count(cut1_vector)<2)){
            last2 = it + i - 1;
            cut2_vector.assign(first1, last2);
            m_histOnePlusCounts.incr_count(cut2_vector);
        }
      }
    }
 }

}

double LangModel::get_prob_witten_bell(const vector<int>& ngram) const {
  double retProb = 1.0;
  //  Don't count epsilon.
  int vocSize = m_symTable->size() - 1;

  //
  //  BEGIN_LAB
  //
  //  This routine should return an n-gram probability smoothed
  //  with Witten-Bell smoothing.
  //
  //  Input:
  //      "m_n" = the value of "n" for the n-gram model.
  //      "ngram" = the input n-gram, expressed as a vector of integers.
  //          This will be of length "m_n".
  //      "m_predCounts" = object holding "regular" counts for each n-gram.
  //      "m_histCounts" = object holding "history" counts for each n-gram.
  //      "m_histOnePlusCounts" = object holding number of unique words
  //          following each n-gram history in the training data.
  //      "vocSize" = vocabulary size.
  //
  //      To access the count of an n-gram, you can do something like:
  //
  //      int predCnt = m_predCounts.get_count(ngram);
  //
  //      where "ngram" is of type "vector<int>".
  //
  //  Output:
  //      "retProb" should be set to the smoothed n-gram probability
  //          of the last word in the n-gram given the previous words.
  //
  double prob_eps = 1.0 / vocSize;    
  double acc_histCounts;   // acc_counts for 1gram, 2grams, ..., ngrams
  double acc_histOnePlusCounts; // acc_histOnePlusCounts for 1gram, 2grams, ..., ngrams
  vector<int> word_idx;

  for (int k = 0 ; k < vocSize ; k++ ) {
      
      word_idx.assign(1,k);
      acc_histCounts += m_histCounts.get_count(word_idx);  
      acc_histOnePlusCounts += m_histOnePlusCounts.get_count(word_idx);  
  }
  // for (int i = 1; i < m_n; i++ ){
  //   vector<int> c_iter(ngram.begin()+i, ngram.end()-1);
  //   acc_histCounts[i] += m_histCounts.get_count(c_iter);
  //   acc_histOnePlusCounts[i] += m_histOnePlusCounts.get_count(c_iter);
  // }
  double p[m_n+1], lambda[m_n];
  for (int i = 0; i < m_n+1; i++ ){
    vector<int> c_iter(ngram.begin()+i, ngram.end());
    if ( (i+1)==m_n ) {
      p[i] = 1.0 / vocSize;
      lambda[i] = acc_histCounts*1.0 / (acc_histCounts + acc_histOnePlusCounts);
    }
    else {
      vector<int> c_iter1(ngram.begin()+i+1, ngram.end());
      p[i] = m_predCounts.get_count(c_iter)/m_predCounts.get_count(c_iter1);
      lambda[i] = m_histCounts.get_count(c_iter)*1.0 / (m_histCounts.get_count(c_iter) + m_histOnePlusCounts.get_count(c_iter));
    } 
    
  }
     
  for (int i = m_n;  i > 0; i--){
    if (i>0) {
      retProb += lambda[i]*p[i] + (1-lambda[i])*p[i-1];
    }
    else{
      retProb += lambda[i]*p[i] + (1-lambda[i])*prob_eps;
    }

  }

  return retProb;
}

double LangModel::get_prob(const vector<int>& ngram) const {
  if ((ngram.size() < 1) || ((int)ngram.size() > m_n))
    throw runtime_error("Invalid n-gram size.");
  return get_prob_witten_bell(ngram);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
