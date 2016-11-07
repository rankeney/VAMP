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

#ifndef JSON_H
#define JSON_H

#include <cstdlib>
#include <sstream>
#include <vector>
#include <string>

#define ENDL "\n"

using namespace std;

class JsonNode {
public:
  string name(void) { return nodeName; }
  string as_string(void) { return nodeData; }
  bool as_bool(void) { return (nodeData == "true"); }
  int as_int(void) { return atoi(nodeData.data()); }

  string nodeName;
  string nodeData;
};

class Json
{
public:
    Json(std::ostringstream *errStr) : jsonErr(errStr)
    {
    }

    void ParseJson(string str);
    void ParseArray(string str, vector<JsonNode> &array);

    vector<JsonNode> jsonNodes;

private :
  char getCh(void);
  char nextCh(void);

  std::ostringstream *jsonErr;
  const char *ptr;
  int line;
};

#endif // JSON_H
