/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 *	 Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is globalmenu-extension.
 *
 * The Initial Developer of the Original Code is
 * Canonical Ltd.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Coulson <chris.coulson@canonical.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

#ifndef _U_DEBUG_H
#define _U_DEBUG_H

#include <nsStringAPI.h>
#include <nsXPCOM.h>

#if defined DEBUG_chrisccoulson && !defined DEBUG
#undef NS_ASSERTION
#undef NS_WARN_IF_FALSE
#undef NS_WARNING

#define NS_ASSERTION(expr, str)                               \
  do {                                                        \
    if (!(expr)) {                                            \
      NS_DebugBreak(NS_DEBUG_ASSERTION, str, #expr, __FILE__, __LINE__); \
    }                                                         \
  } while(0)

#define NS_WARN_IF_FALSE(_expr,_msg)                          \
  do {                                                        \
    if (!(_expr)) {                                           \
      NS_DebugBreak(NS_DEBUG_WARNING, _msg, #_expr, __FILE__, __LINE__); \
    }                                                         \
  } while(0)

#define NS_WARNING(str)                                       \
  NS_DebugBreak(NS_DEBUG_WARNING, str, nsnull, __FILE__, __LINE__)
#endif

#ifdef DEBUG
#define DEBUG_chrisccoulson
#endif

#ifdef DEBUG_chrisccoulson
class FunctionTracer
{
public:
  FunctionTracer(const char *funcName, const char *extra): mFuncName(funcName), mExtra(extra)
  {
    if (getenv("GLOBAL_MENU_VERBOSE")) {
      printf("%*s=== globalmenu-extension: Entering %s (%s) ===\n", sDepth, "", mFuncName.get(), mExtra.get());
    }
    sDepth++;
  }

  FunctionTracer(const char *funcName): mFuncName(funcName)
  {
    if (getenv("GLOBAL_MENU_VERBOSE")) {
      printf("%*s=== globalmenu-extension: Entering %s ===\n", sDepth, "", mFuncName.get());
    }
    sDepth++;
  }

  ~FunctionTracer()
  {
    sDepth--;
    if (getenv("GLOBAL_MENU_VERBOSE")) {
      if (mExtra.Length() > 0) {
        printf("%*s=== globalmenu-extension: Leaving %s (%s) ===\n", sDepth, "", mFuncName.get(), mExtra.get());
      } else {
        printf("%*s=== globalmenu-extension: Leaving %s ===\n", sDepth, "", mFuncName.get());
      }
    }
  }

  static PRUint32 sDepth;
private:
  nsCString mFuncName;
  nsCString mExtra;
};

#define DEBUG_WITH_THIS_MENUOBJECT(format...)                 \
  if (getenv("GLOBAL_MENU_VERBOSE")) {                        \
    nsAutoString id;                                          \
    nsCAutoString cid;                                        \
    mContent->GetAttr(kNameSpaceID_None, uWidgetAtoms::id, id); \
    CopyUTF16toUTF8(id, cid);                                 \
    char *str;                                                \
    asprintf(&str, format);                                   \
    printf("%*s* globalmenu-extension %s: %s (id: %s)\n", FunctionTracer::sDepth, "", __PRETTY_FUNCTION__, str, cid.get()); \
    free(str);                                                \
  }

#define DEBUG_WITH_CONTENT(content, format...)                \
  if (getenv("GLOBAL_MENU_VERBOSE")) {                        \
    nsAutoString id;                                          \
    nsCAutoString cid;                                        \
    content->GetAttr(kNameSpaceID_None, uWidgetAtoms::id, id); \
    CopyUTF16toUTF8(id, cid);                                 \
    char *str;                                                \
    asprintf(&str, format);                                   \
    printf("%*s* globalmenu-extension %s: %s (id: %s)\n", FunctionTracer::sDepth, "", __PRETTY_FUNCTION__, str, cid.get()); \
    free(str);                                                \
  }

#define DEBUG_CSTR_FROM_UTF16(str) __extension__({            \
  nsCAutoString cstr;                                         \
  CopyUTF16toUTF8(str, cstr);                                 \
  cstr.get();                                                 \
})

#define TRACE_WITH_THIS_MENUOBJECT()                          \
  char *_id_s;                                                \
  {                                                           \
    nsAutoString _id;                                         \
    mContent->GetAttr(kNameSpaceID_None, uWidgetAtoms::id, _id); \
    nsCAutoString _cid;                                       \
    CopyUTF16toUTF8(_id, _cid);                               \
    asprintf(&_id_s, "id: %s", _cid.get());                   \
  }                                                           \
  FunctionTracer _marker(__PRETTY_FUNCTION__, _id_s);         \
  free(_id_s);

#define TRACE_WITH_CONTENT(content)                           \
  char *_id_s;                                                \
  {                                                           \
    nsAutoString _id;                                         \
    content->GetAttr(kNameSpaceID_None, uWidgetAtoms::id, _id); \
    nsCAutoString _cid;                                       \
    CopyUTF16toUTF8(_id, _cid);                               \
    asprintf(&_id_s, "id: %s", _cid.get());                   \
  }                                                           \
  FunctionTracer _marker(__PRETTY_FUNCTION__, _id_s);         \
  free(_id_s);

#define TRACE()                                               \
  FunctionTracer _marker(__PRETTY_FUNCTION__);
#else
#define DEBUG_WITH_THIS_MENUOBJECT(format...)
#define DEBUG_WITH_CONTENT(content, format...)
#define DEBUG_CSTR_FROM_UTF16(str)
#define TRACE_WITH_THIS_MENUOBJECT()
#define TRACE_WITH_CONTENT(content)
#define TRACE()
#endif

#endif
