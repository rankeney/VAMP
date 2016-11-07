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

#ifndef VAMP_OSTREAM_H
#define VAMP_OSTREAM_H
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>

#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
//#include "llvm/ADT/IntrusiveRefCntPtr.h"
//#include "llvm/ADT/StringRef.h"

using namespace llvm;
using namespace std;

#ifndef LLVM_OVERRIDE
#define LLVM_OVERRIDE override
#endif

/// raw_string_ostream - A raw_ostream that writes to an std::string.  This is a
/// simple adaptor class. This class does not encounter output errors.
class vamp_string_ostream : public raw_ostream {
  std::string &OS;

  /// write_impl - See raw_ostream::write_impl.
  virtual void write_impl(const char *Ptr, size_t Size) LLVM_OVERRIDE;

  /// current_pos - Return the current position within the stream, not
  /// counting the bytes currently in the buffer.
  virtual uint64_t current_pos() const LLVM_OVERRIDE { return OS.size(); }

  /// Changes the foreground color of text that will be output from this point
  /// forward.
  /// @param Color ANSI color to use, the special SAVEDCOLOR can be used to
  /// change only the bold attribute, and keep colors untouched
  /// @param Bold bold/brighter text, default false
  /// @param BG if true change the background, default: change foreground
  /// @returns itself so it can be used within << invocations
  virtual raw_ostream &changeColor(enum Colors Color,
                                   bool Bold = false,
                                   bool BG = false) {
    flush();
    ostringstream ansi;
    char color;
    if (Color == SAVEDCOLOR)
      // Use saved color from last changeColor
      color = static_cast<char>('0' + savedColor);
    else
    {
      color = static_cast<char>('0' + Color);
      savedColor = Color;
    }
    ansi << "\x1b[" << static_cast<char>('3' + BG) << color << 'm';
    OS += ansi.str();
    flush();
    (void)Color;
    (void)Bold;
    (void)BG;
    return *this;
  }

  /// Resets the colors to terminal defaults. Call this when you are done
  /// outputting colored text, or before program exit.
  virtual raw_ostream &resetColor()
  {
    flush();
    OS += std::string("\x1b[0m");
    flush();
    // Reset saved color to white
    savedColor = WHITE;
    return *this;
  }

  /// Reverses the forground and background colors.
  virtual raw_ostream &reverseColor() { return *this; }

  /// This function determines if this stream is connected to a "tty" or
  /// "console" window. That is, the output would be displayed to the user
  /// rather than being put on a pipe or stored in a file.
  /// Since we're outputting to a string, always allow colors
  virtual bool is_displayed() const { return true; }

public:
  explicit vamp_string_ostream(std::string &O) : OS(O) { savedColor = WHITE; }
  ~vamp_string_ostream();

  /// str - Flushes the stream contents to the target string and returns
  ///  the string's reference.
  std::string& str() {
    flush();
    return OS;
  }

private:
  /// When changeColor() is called, remember the previous color
  /// When resetColor() is called, reset savedColor to WHITE
  Colors savedColor;
};

#ifdef DEFINE_VAMP_OSTREAM_SOURCE
vamp_string_ostream::~vamp_string_ostream() {
  flush();
}

void vamp_string_ostream::write_impl(const char *Ptr, size_t Size) {
  OS.append(Ptr, Size);
}
#endif
#endif  // VAMP_OSTREAM_H
