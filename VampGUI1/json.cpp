/*  Copyright 2016, Robert Ankeney

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/
*/

#include "json.h"
#ifdef USE_QT
#include <QMessageBox>
#endif

char Json::getCh(void)
{
  if (*ptr == 0)
  {
#ifdef USE_QT
    QMessageBox::critical(0, QString("Error"), QString("Unexpected end of file at line %1 - aborting").arg(line));
#else
    *jsonErr << "Unexpected end of file at line " << line << " - aborting" << ENDL;
#endif
    throw(1);
  }

  return *ptr++;
}

char Json::nextCh(void)
{
  char ch;
  bool tryAgain;

  ch = getCh();

  do
  {
    tryAgain = false;
    if ((ch == '/') && (getCh() == '*'))
    {
      // Skip comment
      while ((getCh() != '/') || (*(ptr - 2) != '*')) ;
      tryAgain = true;
      ch = getCh();
      ch = getCh();
    }

    // Ignore white space
    while ((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'))
    {
      ch = getCh();
      tryAgain = true;
    }
  } while (tryAgain);

  if (ch == '\\')
  {
    // Ignore escape and return next char
    ch = getCh();
  }

  return ch;
}

void Json::ParseJson(string str)
{
  char ch;

  line = 1;
  ptr = str.data();

  ch = nextCh();
  if (ch != '{')
  {
#ifdef USE_QT
    QMessageBox::critical(0, QString("Error"), QString("Expected '{' at line %1 - aborting").arg(line));
#else
    *jsonErr << "Expected '{' at line " << line << " - aborting" << ENDL;
#endif
    throw(1);
  }

  do
  {
    JsonNode jsonNode;

    // Get node name
    ch = nextCh();
    if (ch != '"')
    {
#ifdef USE_QT
      QMessageBox::critical(0, QString("Error"), QString("Expected '\"' at line %1 - aborting").arg(line));
#else
      *jsonErr << "Expected '\"' at line " << line << " - aborting" << ENDL;
#endif
      throw(1);
    }

    ch = getCh();
    while (ch != '"')
    {
      jsonNode.nodeName += ch;
      ch = getCh();
    }

    if (nextCh() != ':')
    {
#ifdef USE_QT
      QMessageBox::critical(0, QString("Error"), QString("Expected ':' at line %1 - aborting").arg(line));
#else
      *jsonErr << "Expected ':' at line " << line << " - aborting" << ENDL;
#endif
      throw(1);
    }

    // Now get node value
    int arrayDepth = 0;

    do {
      ch = nextCh();
      jsonNode.nodeData += ch;
      if (ch == '[')
      {
        ++arrayDepth;
      }
      else
      if (ch == ']')
      {
        if (arrayDepth-- == 0)
        {
#ifdef USE_QT
          QMessageBox::critical(0, QString("Error"), QString("Unexpected ']' at line %1 - aborting").arg(line));
#else
          *jsonErr << "Unexpected ']' at line " << line << " - aborting" << ENDL;
#endif
          throw(1);
        }
      }
      else
      if (ch == '"')
      {
        // Get string
        do {
          ch = getCh();
          jsonNode.nodeData += ch;
        } while (ch != '"');
      }
    } while ((arrayDepth > 0) || ((ch != ',') && (ch != '}')));

    if (jsonNode.nodeData[0] == '"')
    {
      // Remove quotes and ',' or '}'
      jsonNode.nodeData =
           jsonNode.nodeData.substr(1, jsonNode.nodeData.size() - 3);
    }
    else
    {
      // Remove ',' or '}'
      jsonNode.nodeData.resize(jsonNode.nodeData.size() - 1);
    }
    jsonNodes.push_back(jsonNode);
  } while (ch == ',');

  if (ch != '}')
  {
#ifdef USE_QT
    QMessageBox::critical(0, QString("Error"), QString("Expected '}' at line %1 - aborting").arg(line));
#else
    *jsonErr << "Expected '}' at line " << line << " - aborting" << ENDL;
#endif
    throw(1);
  }

}

void Json::ParseArray(string str, vector<JsonNode> &array)
{
  char ch;

  if (str == "[]")
  {
    // Empty array - return
    return;
  }

  ptr = str.data();
  if (nextCh() != '[')
  {
#ifdef USE_QT
    QMessageBox::critical(0, QString("Error"), QString("Expected '[' in array - aborting"));
#else
    *jsonErr << "Expected '[' in array" << " - aborting" << ENDL;
#endif
    throw(1);
  }

  // Now get node value
  int arrayDepth = 0;

  do {
    JsonNode node;

    do {
      ch = nextCh();
      node.nodeData += ch;

      if (ch == '[')
      {
        ++arrayDepth;
      }
      else
      if (ch == ']')
      {
        while (arrayDepth-- && (*ptr == ']'))
        {
          ch = nextCh();
          node.nodeData += ch;
        }
        if (arrayDepth == 0)
        {
          ch = nextCh();
          node.nodeData += ch;
        }
      }
      else
      if (ch == '"')
      {
        // Get string
        do {
          ch = getCh();
          node.nodeData += ch;
        } while (ch != '"');
      }
    } while ((arrayDepth > 0) || ((ch != ',') && (ch != ']')));

    if (node.nodeData[0] == '"')
    {
      // Remove quotes and ',' or '}'
      node.nodeData = node.nodeData.substr(1, node.nodeData.size() - 3);
    }
    else
    {
      // Remove ',' or ']'
      node.nodeData.resize(node.nodeData.size() - 1);
    }
    array.push_back(node);
    arrayDepth = 0;
  } while (ch != ']');
}
