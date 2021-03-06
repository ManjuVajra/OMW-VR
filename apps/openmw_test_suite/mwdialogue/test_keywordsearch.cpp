#include <gtest/gtest.h>
#include "apps/openmw/mwdialogue/keywordsearch.hpp"

struct KeywordSearchTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(KeywordSearchTest, keyword_test_conflict_resolution)
{
    // test to make sure the longest keyword in a chain of conflicting keywords gets chosen
    MWDialogue::KeywordSearch<std::string, int> search;
    search.seed("foo bar", 0);
    search.seed("bar lock", 0);
    search.seed("lock switch", 0);

    std::string text = "foo bar lock switch";

    std::vector<MWDialogue::KeywordSearch<std::string, int>::Match> matches;
    search.highlightKeywords(text.begin(), text.end(), matches);

    // Should contain: "foo bar", "lock switch"
    EXPECT_EQ (matches.size() , 2);
    EXPECT_EQ (std::string(matches.front().mBeg, matches.front().mEnd) , "foo bar");
    EXPECT_EQ (std::string(matches.rbegin()->mBeg, matches.rbegin()->mEnd) , "lock switch");
}

TEST_F(KeywordSearchTest, keyword_test_conflict_resolution2)
{
    MWDialogue::KeywordSearch<std::string, int> search;
    search.seed("the dwemer", 0);
    search.seed("dwemer language", 0);

    std::string text = "the dwemer language";

    std::vector<MWDialogue::KeywordSearch<std::string, int>::Match> matches;
    search.highlightKeywords(text.begin(), text.end(), matches);

    EXPECT_EQ (matches.size() , 1);
    EXPECT_EQ (std::string(matches.front().mBeg, matches.front().mEnd) , "dwemer language");
}


TEST_F(KeywordSearchTest, keyword_test_conflict_resolution3)
{
    // testing that the longest keyword is chosen, rather than maximizing the
    // amount of highlighted characters by highlighting the first and last keyword
    MWDialogue::KeywordSearch<std::string, int> search;
    search.seed("foo bar", 0);
    search.seed("bar lock", 0);
    search.seed("lock so", 0);

    std::string text = "foo bar lock so";

    std::vector<MWDialogue::KeywordSearch<std::string, int>::Match> matches;
    search.highlightKeywords(text.begin(), text.end(), matches);

    EXPECT_EQ (matches.size() , 1);
    EXPECT_EQ (std::string(matches.front().mBeg, matches.front().mEnd) , "bar lock");
}


TEST_F(KeywordSearchTest, keyword_test_utf8_word_begin)
{
    // We make sure that the search works well even if the character is not ASCII
    MWDialogue::KeywordSearch<std::string, int> search;
    search.seed("??tats", 0);
    search.seed("??rradi??s", 0);
    search.seed("??a nous d????ois", 0);
    search.seed("ois", 0);

    std::string text = "les nations unis ont r??unis le monde entier, ??tats units inclus pour parler du probl??me des gens ??rradi??s et ??a nous d????ois";

    std::vector<MWDialogue::KeywordSearch<std::string, int>::Match> matches;
    search.highlightKeywords(text.begin(), text.end(), matches);

    EXPECT_EQ (matches.size() , 3);
    EXPECT_EQ (std::string( matches[0].mBeg, matches[0].mEnd) , "??tats");
    EXPECT_EQ (std::string( matches[1].mBeg, matches[1].mEnd) , "??rradi??s");
    EXPECT_EQ (std::string( matches[2].mBeg, matches[2].mEnd) , "??a nous d????ois");
}

TEST_F(KeywordSearchTest, keyword_test_non_alpha_non_whitespace_word_begin)
{
    // We make sure that the search works well even if the separator is not a whitespace
    MWDialogue::KeywordSearch<std::string, int> search;
    search.seed("Report to caius cosades", 0);



    std::string text = "I was told to \"Report to caius cosades\"";

    std::vector<MWDialogue::KeywordSearch<std::string, int>::Match> matches;
    search.highlightKeywords(text.begin(), text.end(), matches);

    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(std::string(matches[0].mBeg, matches[0].mEnd), "Report to caius cosades");
}

TEST_F(KeywordSearchTest, keyword_test_russian_non_ascii_before)
{
    // We make sure that the search works well even if the separator is not a whitespace with russian chars
    MWDialogue::KeywordSearch<std::string, int> search;
    search.seed("???????????????? ?????? ????????????????", 0);

    std::string text = "??????? ????. ?? ?????? ??????????????. ???? ???????? ?????? ??????, ?????? ???????????? ?????????????????? ?????? ??????????????????? ?? ?????? ???? ?????????????????";

    std::vector<MWDialogue::KeywordSearch<std::string, int>::Match> matches;
    search.highlightKeywords(text.begin(), text.end(), matches);

    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(std::string(matches[0].mBeg, matches[0].mEnd), "???????????????? ?????? ????????????????");
}

TEST_F(KeywordSearchTest, keyword_test_russian_ascii_before)
{
    // We make sure that the search works well even if the separator is not a whitespace with russian chars
    MWDialogue::KeywordSearch<std::string, int> search;
    search.seed("???????????????? ?????? ????????????????", 0);

    std::string text = "??????? ????. ?? ?????? ??????????????. ???? ???????? ?????? ??????, ?????? ???????????? '???????????????? ?????? ????????????????'? ?? ?????? ???? ?????????????????";

    std::vector<MWDialogue::KeywordSearch<std::string, int>::Match> matches;
    search.highlightKeywords(text.begin(), text.end(), matches);

    EXPECT_EQ(matches.size(), 1);
    EXPECT_EQ(std::string(matches[0].mBeg, matches[0].mEnd), "???????????????? ?????? ????????????????");
}

