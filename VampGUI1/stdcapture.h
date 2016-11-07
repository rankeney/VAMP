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

#ifndef STDCAPTURE_H
#define STDCAPTURE_H

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string>

class StdCapture
{
public:
    StdCapture(): m_capturing(false), m_init(false), m_oldStdOut(0), m_oldStdErr(0)
    {
        m_pipe[OUT_READ] = 0;
        m_pipe[OUT_WRITE] = 0;
        m_pipe[ERR_READ] = 0;
        m_pipe[ERR_WRITE] = 0;
        if (_pipe(&m_pipe[OUT_READ], 65536, O_BINARY) == -1)
            return;
        if (_pipe(&m_pipe[ERR_READ], 65536, O_BINARY) == -1)
            return;
        m_oldStdOut = dup(fileno(stdout));
        m_oldStdErr = dup(fileno(stderr));
        if (m_oldStdOut == -1 || m_oldStdErr == -1)
            return;

        m_init = true;
    }

    ~StdCapture()
    {
        if (m_capturing)
        {
            EndCapture();
        }
        if (m_oldStdOut > 0)
            close(m_oldStdOut);
        if (m_oldStdErr > 0)
            close(m_oldStdErr);
        if (m_pipe[OUT_READ] > 0)
            close(m_pipe[OUT_READ]);
        if (m_pipe[OUT_WRITE] > 0)
            close(m_pipe[OUT_WRITE]);
        if (m_pipe[ERR_READ] > 0)
            close(m_pipe[ERR_READ]);
        if (m_pipe[ERR_WRITE] > 0)
            close(m_pipe[ERR_WRITE]);
    }


    void BeginCapture()
    {
        if (!m_init)
            return;
        if (m_capturing)
            EndCapture();
        fflush(stdout);
        fflush(stderr);
        dup2(m_pipe[OUT_WRITE], fileno(stdout));
        dup2(m_pipe[ERR_WRITE], fileno(stderr));
        m_capturing = true;
    }

    bool EndCapture()
    {
        if (!m_init)
            return false;
        if (!m_capturing)
            return false;
        fflush(stdout);
        fflush(stderr);
        dup2(m_oldStdOut, fileno(stdout));
        dup2(m_oldStdErr, fileno(stderr));
        m_outCaptured.clear();
        m_errCaptured.clear();

        std::string outBuf;
        std::string errBuf;
        const int bufSize = 1024;
        outBuf.resize(bufSize);
        errBuf.resize(bufSize);
        int outBytesRead = 0;
        int errBytesRead = 0;
        if (!eof(m_pipe[OUT_READ]))
        {
            outBytesRead = read(m_pipe[OUT_READ], &(*outBuf.begin()), bufSize);
        }
        while(outBytesRead == bufSize)
        {
            m_outCaptured += outBuf;
            outBytesRead = 0;
            if (!eof(m_pipe[OUT_READ]))
            {
                outBytesRead = read(m_pipe[OUT_READ], &(*outBuf.begin()), bufSize);
            }
        }
        if (outBytesRead > 0)
        {
            outBuf.resize(outBytesRead);
            m_outCaptured += outBuf;
        }

        if (!eof(m_pipe[ERR_READ]))
        {
            errBytesRead = read(m_pipe[ERR_READ], &(*errBuf.begin()), bufSize);
        }
        while(errBytesRead == bufSize)
        {
            m_errCaptured += errBuf;
            errBytesRead = 0;
            if (!eof(m_pipe[ERR_READ]))
            {
                errBytesRead = read(m_pipe[ERR_READ], &(*errBuf.begin()), bufSize);
            }
        }
        if (errBytesRead > 0)
        {
            errBuf.resize(errBytesRead);
            m_errCaptured += errBuf;
        }

        return true;
    }

    std::string GetOutCapture() const
    {
        std::string::size_type idx = m_outCaptured.find_last_not_of("\r\n");
        if (idx == std::string::npos)
        {
            return m_outCaptured;
        }
        else
        {
            return m_outCaptured.substr(0, idx+1);
        }
    }

    std::string GetErrCapture() const
    {
        std::string::size_type idx = m_errCaptured.find_last_not_of("\r\n");
        if (idx == std::string::npos)
        {
            return m_errCaptured;
        }
        else
        {
            return m_errCaptured.substr(0, idx+1);
        }
    }

private:
    enum PIPES { OUT_READ, OUT_WRITE, ERR_READ, ERR_WRITE };
    int m_pipe[4];
    int m_oldStdOut;
    int m_oldStdErr;
    bool m_capturing;
    bool m_init;
    std::string m_outCaptured;
    std::string m_errCaptured;
};
#endif // STDCAPTURE_H
