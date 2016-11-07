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

#include "vamp_rewrite.h"

vampRewrite::vampRewrite()
{
  rewriteOccurrence = 0;
}

#ifdef DEBUG_REWRITE_NODES
void vampRewrite::showRewriteNodes()
{
  cout << "Rewrite Nodes:" << ENDL;
  for (int i = 0; i < rewriteNodes.size(); i++)
  {
    cout << i << ": " << rewriteNodes[i].lhsLoc.getRawEncoding() << ", " <<
                         rewriteNodes[i].rhsLoc.getRawEncoding() << "; occ=" <<
                         rewriteNodes[i].occurrence << "; count=" <<
                         rewriteNodes[i].count << "; lhs=\"" <<
                         rewriteNodes[i].lhsString << "\"; rhs=\"" <<
                         rewriteNodes[i].rhsString << "\"" << ENDL;
  }
}
#endif

// Add a left/right pair rewriter node at specified left/right locations
// with specified left/right rewrite strings
void vampRewrite::addRewriteNode(SourceLocation lhsLoc,
                                 string lhsString,
                                 SourceLocation rhsLoc,
                                 string rhsString)
{
#ifdef REWRITER_DEBUG
  COUT << "lhs loc = " << lhsLoc.getRawEncoding() <<
          ", rhs loc = " << rhsLoc.getRawEncoding() <<
          "lhs str =" << lhsString <<
          "rhs str =" << rhsString <<
          "; occ=" << rewriteOccurrence << ENDL;
#endif

  REWRITE_NODE rewriteNode;

  rewriteNode.lhsLoc = lhsLoc;
  rewriteNode.lhsString = lhsString;
  rewriteNode.rhsLoc = rhsLoc;
  rewriteNode.rhsString = rhsString;
  rewriteNode.occurrence = rewriteOccurrence++;
  rewriteNode.count = 0;
  rewriteNodes.push_back(rewriteNode);
#ifdef DEBUG_REWRITE_NODES
  showRewriteNodes();
#endif
}

// Create a MCDC rewrite node
// This implies we have the left side of the expression, so push
// that information onto the stack and pop when we have the right
// side of the expression
void vampRewrite::addMCDCLeftNode(SourceLocation lhsLoc,
                                  string lhsString)
{
  REWRITE_NODE rewriteNode;

  rewriteNode.lhsLoc = lhsLoc;
  rewriteNode.lhsString = lhsString;
  rewriteNode.occurrence = rewriteOccurrence++;
  rewriteNode.count = 0;
  mcdcRewriteNodes.push_back(rewriteNode);
}

// Complete a MCDC rewrite node
// This implies we have the right side of the expression, so pop
// the left side information from the stack and and add the right
// side information. Push the result onto the rewriter stack.
void vampRewrite::addMCDCRightNode(SourceLocation rhsLoc,
                                   string rhsString)
{
  REWRITE_NODE rewriteNode;

  // Pop most recent LHS info
  rewriteNode = mcdcRewriteNodes.back();
  // Remove from stack
  mcdcRewriteNodes.pop_back();

  // Add RHS info
  rewriteNode.rhsLoc = rhsLoc;
  rewriteNode.rhsString = rhsString;

  // Save to rewrite list
  rewriteNodes.push_back(rewriteNode);
#ifdef DEBUG_REWRITE_NODES
  showRewriteNodes();
#endif

#ifdef REWRITER_DEBUG
  COUT << "MCDC right pop, lhsLoc=" << rewriteNode.lhsLoc.getRawEncoding() <<
          ", rhsLoc=" << rhsLoc.getRawEncoding() << "; " <<
          " lhs=" << rewriteNode.lhsString << ", " <<
          " rhs=" << rhsString << ", " <<
          " occ=" << rewriteNode.occurrence << " :" << ENDL;
#endif
}

void vampRewrite::InsertText(SourceLocation loc, string text)
{
  REWRITE_NODE rewriteNode;

  rewriteNode.lhsLoc = loc;
  rewriteNode.lhsString = text;
  rewriteNode.rhsLoc = loc;
  rewriteNode.rhsString = "";
  rewriteNode.occurrence = rewriteOccurrence++;
  rewriteNode.count = 0;
  rewriteNodes.push_back(rewriteNode);
#ifdef DEBUG_REWRITE_NODES
  showRewriteNodes();
#endif
}

void vampRewrite::ReplaceText(SourceLocation loc, int count, string text)
{
  REWRITE_NODE rewriteNode;

  rewriteNode.lhsLoc = loc;
  rewriteNode.lhsString = text;
  rewriteNode.rhsLoc = loc;
  rewriteNode.rhsString = "";
  rewriteNode.occurrence = rewriteOccurrence++;
  rewriteNode.count = count;
  rewriteNodes.push_back(rewriteNode);
#ifdef DEBUG_REWRITE_NODES
cout << "Text = \"" << text << "\"\n";
cout << "Count = " << count << ", node count = " << rewriteNode.count << ENDL;
  showRewriteNodes();
#endif
}

void vampRewrite::ProcessRewriteNodes(Rewriter &Rewrite)
{
#ifdef REWRITER_DEBUG
  COUT << "PRESORT:" << ENDL;
  for (int i = 0; i < rewriteNodes.size(); i++)
  {
    COUT << i << ": " << rewriteNodes[i].lhsLoc.getRawEncoding() << ", " <<
                         rewriteNodes[i].rhsLoc.getRawEncoding() << "; " <<
                         rewriteNodes[i].occurrence << "; lhs=" <<
                         rewriteNodes[i].lhsString << "; rhs=" <<
                         rewriteNodes[i].rhsString << ENDL;
  }
#endif

  // First rewrite the left-hand nodes
  sort(rewriteNodes.begin(), rewriteNodes.end(), rewriteLocLeftCompare());

#ifdef REWRITER_DEBUG
  COUT << "POSTSORT:" << ENDL;
  for (int i = 0; i < rewriteNodes.size(); i++)
  {
    COUT << i << ": " << rewriteNodes[i].lhsLoc.getRawEncoding() << ", " <<
                         rewriteNodes[i].rhsLoc.getRawEncoding() << ", " <<
                         rewriteNodes[i].occurrence << "; lhs=" <<
                         rewriteNodes[i].lhsString << "; rhs=" <<
                         rewriteNodes[i].rhsString << ENDL;
  }
#endif

  vector<REWRITE_NODE>::iterator ip;
  for (ip = rewriteNodes.begin();
       ip < rewriteNodes.end(); )
  {
    REWRITE_NODE node;

    node = *ip;
    string lhs = node.lhsString;
    int count = node.count;
    ++ip;

    while ((ip < rewriteNodes.end()) && (node.lhsLoc == ip->lhsLoc))
    {
      lhs += ip->lhsString;
      count += ip->count;
      ip->count = 0;

#ifdef REWRITER_DEBUG
      if ((node.rhsLoc == ip->rhsLoc) &&
          (node.lhsString != "") &&
          (ip->lhsString != ""))
      {
        COUT << "LBAD NEWS; LHS =" << node.lhsLoc.getRawEncoding() <<
                "lhs node = " << node.lhsString << ENDL;
        COUT << "           RHS =" << node.rhsLoc.getRawEncoding() <<
                "ip->node = " << ip->rhsString << ENDL;
      }
      node = *ip;
#endif
      ++ip;
    }

#ifdef REWRITER_DEBUG
      COUT << "LHS string " << node.lhsLoc.getRawEncoding() << " = " <<
              lhs << ENDL;
#endif
      if (lhs != "")
      {
        if (count)
        {
          Rewrite.ReplaceText(node.lhsLoc, count, lhs);
        }
        else
        {
          Rewrite.InsertText(node.lhsLoc, lhs, true);
        }
      }
      else
      if (count)
      {
        Rewrite.RemoveText(node.lhsLoc, count);
      }
  }

  // FIXME: I don't think we need this any longer!
  // REVISED: It only works this way
  for (int i = 0; i < rewriteNodes.size(); i++)
  {
    rewriteNodes[i].occurrence = i;
  }

  // Now rewrite the right-hand nodes
  sort(rewriteNodes.begin(), rewriteNodes.end(), rewriteLocRightCompare());

#ifdef REWRITER_DEBUG
  COUT << "POSTSORT:" << ENDL;
  for (int i = 0; i < rewriteNodes.size(); i++)
  {
    COUT << i << ": " << rewriteNodes[i].lhsLoc.getRawEncoding() << ", " <<
                         rewriteNodes[i].rhsLoc.getRawEncoding() << ", " <<
                         rewriteNodes[i].occurrence << ENDL;
  }
#endif

  for (ip = rewriteNodes.begin();
       ip < rewriteNodes.end(); )
  {
    REWRITE_NODE node;

    node = *ip;
    string rhs = node.rhsString;
    ++ip;

    while ((ip < rewriteNodes.end()) && (node.rhsLoc == ip->rhsLoc))
    {
      rhs += ip->rhsString;

#ifdef REWRITER_DEBUG
      if ((node.lhsLoc == ip->lhsLoc) &&
          (node.rhsString != "") &&
          (ip->rhsString != ""))
      {
        COUT << "RBAD NEWS; LHS =" << node.lhsLoc.getRawEncoding() <<
                "rhs node = " << node.rhsString << ENDL;
        COUT << "           RHS =" << node.rhsLoc.getRawEncoding() <<
                "ip->node = " << ip->rhsString << ENDL;
      }
      node = *ip;
#endif

      ++ip;
    }

#ifdef REWRITER_DEBUG
    COUT << "RHS string " << node.rhsLoc.getRawEncoding() << " = " <<
            rhs << ENDL;
#endif

    if (rhs != "")
    {
      Rewrite.InsertText(node.rhsLoc, rhs, true);
    }
  }
}

