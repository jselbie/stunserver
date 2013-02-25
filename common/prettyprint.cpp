/*
   Copyright 2011 John Selbie

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


#include "commonincludes.hpp"


static bool IsWhitespace(char ch)
{
    return ((ch == ' ') || (ch == '\t') || (ch == '\v') || (ch == '\r') || (ch == '\n'));
}

static void SplitParagraphIntoWords(const char* pszLine, std::vector<std::string>& listWords)
{
    std::string strWord;

    if (pszLine == NULL)
    {
        return;
    }

    while (*pszLine)
    {
        while ((*pszLine) && IsWhitespace(*pszLine))
        {
            pszLine++;
        }

        if (*pszLine == 0)
        {
            return;
        }

        while ((*pszLine) && IsWhitespace(*pszLine)==false)
        {
            strWord += *pszLine;
            pszLine++;
        }

        listWords.push_back(strWord);
        strWord.clear();
    }
}

static void SplitInputIntoParagraphs(const char* pszInput, std::vector<std::string>& listParagraphs)
{
    // blindly scan to the next \r or \n
    std::string strParagraph;

    if (pszInput == NULL)
    {
        return;
    }

    while (*pszInput)
    {
        while ((*pszInput != '\0') && (*pszInput != '\r') && (*pszInput != '\n'))
        {
            strParagraph += *pszInput;
            pszInput++;
        }

        listParagraphs.push_back(strParagraph);
        strParagraph.clear();

        if (*pszInput == '\r')
        {
            pszInput++;
            if (*pszInput == '\n')
            {
                pszInput++;
            }
        }
        else if (*pszInput == '\n')
        {
            pszInput++;
        }
    }
}

static void PrintParagraph(const char* psz, size_t width)
{
    size_t indent = 0;
    const char *pszProbe = psz;
    std::vector<std::string> listWords;
    bool fLineStart = true;
    std::string strLine;
    std::string strIndent;
    size_t wordcount = 0;
    size_t wordindex = 0;

    if (width <= 0)
    {
        return;
    }

    if (psz==NULL)
    {
        return;
    }

    while ((*pszProbe) && IsWhitespace(*pszProbe))
    {
        indent++;
        pszProbe++;
    }

    SplitParagraphIntoWords(psz, listWords);
    wordcount = listWords.size();

    if (indent >= width)
    {
        indent = width-1;
    }
    for (size_t x = 0; x < indent; x++)
    {
        strIndent += ' ';
    }

    while (wordindex < wordcount)
    {

        if (fLineStart)
        {
            fLineStart = false;
            strLine = strIndent;

            // we always consume a word and put it at the start of a line regardless of the space we have left
            // one day we can consider doing hyphenation...
            strLine += listWords[wordindex];
            wordindex++;
        }
        else
        {
            // do we have enough room to fit including the space?
            size_t newsize = strLine.size() + 1 + listWords[wordindex].size();

            if (newsize <= width)
            {
                strLine += ' ';
                strLine += listWords[wordindex];
                wordindex++;
            }
            else
            {
                // not a fit, we need to start a new line

                // flush whatever we have now
                printf("%s\n", strLine.c_str());

                // flag that we got a new line starting
                fLineStart = true;

            }
        }

    }

    if ((strLine.size() > 0) || (wordcount == 0))
    {
        printf("%s\n", strLine.c_str());
    }



}

void PrettyPrint(const char* pszInput, size_t width)
{
    std::vector<std::string> listParagraphs;
    size_t len;

    SplitInputIntoParagraphs(pszInput, listParagraphs);

    len = listParagraphs.size();
    for (size_t x = 0; x < len; x++)
    {
        PrintParagraph(listParagraphs[x].c_str(), width);
    }
}

