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

#include "mcdcExprTree.h"

mcdcExprTree::mcdcExprTree()
{
  root = NULL;
}

mcdcExprTree::~mcdcExprTree()
{
  destroyTree();
}

void mcdcExprTree::destroyTree(mcdcNode *leaf)
{
  if (leaf != NULL)
  {
    destroyTree(leaf->lhs);
    destroyTree(leaf->rhs);
    delete leaf;
  }
}

void mcdcExprTree::destroyTree()
{
  destroyTree(root);
  root = NULL;
}

bool mcdcExprTree::insertLeaf(int type, int num,
                              binOp *E,
                              int lhsMinLine, int lhsMinCol,
                              int lhsMaxLine, int lhsMaxCol,
                              int rhsMinLine, int rhsMinCol,
                              int rhsMaxLine, int rhsMaxCol)
{
  if (root != NULL)
  {
    return insertLeaf(&root, type, num, E,
                      lhsMinLine, lhsMinCol,
                      lhsMaxLine, lhsMaxCol,
                      rhsMinLine, rhsMinCol,
                      rhsMaxLine, rhsMaxCol);
  }
  else
  {
    root = new mcdcNode;
    root->type = type;
    root->num = num;
    root->E = E;
    root->lhsMinLine = lhsMinLine;
    root->lhsMinCol = lhsMinCol;
    root->lhsMaxLine = lhsMaxLine;
    root->lhsMaxCol = lhsMaxCol;
    root->rhsMinLine = rhsMinLine;
    root->rhsMinCol = rhsMinCol;
    root->rhsMaxLine = rhsMaxLine;
    root->rhsMaxCol = rhsMaxCol;
    root->lhs = NULL;
    root->rhs = NULL;
  }

  return true;
}

bool mcdcExprTree::insertLeaf(mcdcNode **leaf, int type, int num,
                              binOp *E,
                              int lhsMinLine, int lhsMinCol,
                              int lhsMaxLine, int lhsMaxCol,
                              int rhsMinLine, int rhsMinCol,
                              int rhsMaxLine, int rhsMaxCol)
{
  if (*leaf == NULL)
  {
    *leaf = new mcdcNode;
    (*leaf)->type = type;
    (*leaf)->num = num;
    (*leaf)->E = E;
    (*leaf)->lhsMinLine = lhsMinLine;
    (*leaf)->lhsMinCol = lhsMinCol;
    (*leaf)->lhsMaxLine = lhsMaxLine;
    (*leaf)->lhsMaxCol = lhsMaxCol;
    (*leaf)->rhsMinLine = rhsMinLine;
    (*leaf)->rhsMinCol = rhsMinCol;
    (*leaf)->rhsMaxLine = rhsMaxLine;
    (*leaf)->rhsMaxCol = rhsMaxCol;
    (*leaf)->lhs = NULL;
    (*leaf)->rhs = NULL;
  }
  else
  {
    // Search lhs and rhs till we find a Line/Col new node fits within
    if (isWithin(lhsMinLine, lhsMinCol,
                 (*leaf)->lhsMinLine, (*leaf)->lhsMinCol,
                 (*leaf)->lhsMaxLine, (*leaf)->lhsMaxCol))
    {
      if (isWithin(lhsMaxLine, lhsMaxCol,
                   (*leaf)->lhsMinLine, (*leaf)->lhsMinCol,
                   (*leaf)->lhsMaxLine, (*leaf)->lhsMaxCol))
      {
        insertLeaf(&((*leaf)->lhs), type, num, E,
                   lhsMinLine, lhsMinCol,
                   lhsMaxLine, lhsMaxCol,
                   rhsMinLine, rhsMinCol,
                   rhsMaxLine, rhsMaxCol);
      }
      else
      {
#ifdef VAMP_DEBUG
        ERROR_OUT << "Help!  I'm lost\n";
#endif
        return false;
      }
    }
    else
    if (isWithin(rhsMinLine, rhsMinCol,
                 (*leaf)->rhsMinLine, (*leaf)->rhsMinCol,
                 (*leaf)->rhsMaxLine, (*leaf)->rhsMaxCol))
    {
      if (isWithin(rhsMaxLine, rhsMaxCol,
                   (*leaf)->rhsMinLine, (*leaf)->rhsMinCol,
                   (*leaf)->rhsMaxLine, (*leaf)->rhsMaxCol))
      {
        insertLeaf(&((*leaf)->rhs), type, num, E,
                   lhsMinLine, lhsMinCol,
                   lhsMaxLine, lhsMaxCol,
                   rhsMinLine, rhsMinCol,
                   rhsMaxLine, rhsMaxCol);
      }
      else
      {
#ifdef VAMP_DEBUG
        ERROR_OUT << "Help!  I'm *so* lost\n";
#endif
        return false;
      }
    }
    else
    {
#ifdef VAMP_DEBUG
      ERROR_OUT << "Help!  I'm *really* lost\n";
#endif
      return false;
    }
  }

  return true;
}

bool mcdcExprTree::isWithin(int valLine, int valCol, int rangeMinLine, int rangeMinCol, int rangeMaxLine, int rangeMaxCol)
{
  return (((valLine > rangeMinLine) ||
           ((valLine == rangeMinLine) && (valCol >= rangeMinCol))) &&
          ((valLine < rangeMaxLine) ||
           ((valLine == rangeMaxLine) && (valCol <= rangeMaxCol))));
}

int mcdcExprTree::nodeCount(mcdcNode *node)
{
  int cnt = 0;

  if (node->lhs == NULL)
    cnt++;
  else
    cnt += nodeCount(node->lhs);

  if (node->rhs == NULL)
    cnt++;
  else
    cnt += nodeCount(node->rhs);

  return cnt;
}

void mcdcExprTree::mergeStr(std::vector<std::string> *leftStr,
                            std::vector<std::string> *rightStr,
                            std::vector<std::string> *result)
{
  // Merge left and right strings and return in result
  for (int i = 0; i < (*leftStr).size(); i++)
  {
    for (int j = 0; j < (*rightStr).size(); j++)
    {
      result->push_back((*leftStr)[i] + (*rightStr)[j]);
    }
  }
}

void mcdcExprTree::getFalse(mcdcNode *node, std::vector<std::string> *str)
{
  std::vector<std::string> leftStr, rightStr;
  std::string dashes = "--------------------------------";

  if (node->lhs == NULL)
  {
    leftStr.push_back("0");
  }
  else
  {
    getFalse(node->lhs, &leftStr);
  }

  if (node->type)
  {
    // This was an AND - return dashes for remainder
    int cnt = (node->rhs == NULL) ? 1 : nodeCount(node->rhs);
    std::string r = dashes.substr(0, cnt);
    rightStr.push_back(r);
    //rightStr.push_back(dashes.substr(0, cnt));

    
    // Merge 0 && dash strings and return result in str
    mergeStr(&leftStr, &rightStr, str);

    // Now handle 1 && 0 case
    leftStr.clear();
    rightStr.clear();

    if (node->lhs == NULL)
    {
      leftStr.push_back("1");
    }
    else
    {
      getTrue(node->lhs, &leftStr);
    }

    if (node->rhs == NULL)
    {
      rightStr.push_back("0");
    }
    else
    {
      getFalse(node->rhs, &rightStr);
    }
  }
  else
  {
    // This was an OR - return 0 for remainder
    if (node->rhs == NULL)
    {
      rightStr.push_back("0");
    }
    else
    {
      getFalse(node->rhs, &rightStr);
    }
  }

  // Merge left and right strings and return result in str
  mergeStr(&leftStr, &rightStr, str);
}

void mcdcExprTree::getTrue(mcdcNode *node, std::vector<std::string> *str)
{
  std::vector<std::string> leftStr, rightStr;
  std::string dashes = "----------------";

  if (node->lhs == NULL)
  {
    leftStr.push_back("1");
  }
  else
  {
    getTrue(node->lhs, &leftStr);
  }

  if (node->type)
  {
    // This was an AND - remainder must be true
    if (node->rhs == NULL)
    {
      rightStr.push_back("1");
    }
    else
    {
      getTrue(node->rhs, &rightStr);
    }
  }
  else
  {
    // This was an OR - remainder must be dashes
    int cnt = (node->rhs == NULL) ? 1 : nodeCount(node->rhs);
    //std::string r = dashes.substr(0, cnt);
    //rightStr.push_back(r);
    rightStr.push_back(dashes.substr(0, cnt));

    // Merge 1 || dash strings and return result in str
    mergeStr(&leftStr, &rightStr, str);

    // Now handle 0 || 1 case
    leftStr.clear();
    rightStr.clear();

    if (node->lhs == NULL)
    {
      leftStr.push_back("0");
    }
    else
    {
      getFalse(node->lhs, &leftStr);
    }

    if (node->rhs == NULL)
    {
      rightStr.push_back("1");
    }
    else
    {
      getTrue(node->rhs, &rightStr);
    }
  }

  // Merge left and right strings and return result in str
  mergeStr(&leftStr, &rightStr, str);
}

