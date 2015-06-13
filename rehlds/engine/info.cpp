/*
*
*    This program is free software; you can redistribute it and/or modify it
*    under the terms of the GNU General Public License as published by the
*    Free Software Foundation; either version 2 of the License, or (at
*    your option) any later version.
*
*    This program is distributed in the hope that it will be useful, but
*    WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*    In addition, as a special exception, the author gives permission to
*    link the code of this program with the Half-Life Game Engine ("HL
*    Engine") and Modified Game Libraries ("MODs") developed by Valve,
*    L.L.C ("Valve").  You must obey the GNU General Public License in all
*    respects for all of the code used other than the HL Engine and MODs
*    from Valve.  If you modify this file, you may extend this exception
*    to your version of the file, but you are not obligated to do so.  If
*    you do not wish to do so, delete this exception statement from your
*    version.
*
*/

#include "precompiled.h"

// NOTE: This file contains a lot of fixes that are not covered by REHLDS_FIXES define.
// TODO: Most of the Info_ functions can be speedup via removing unneded copy of key and values.
// TODO: We have a problem with Q_strcpy, because it maps to strcpy which have undefined behavior when strings overlaps (possibly we need to use memmove solution here)

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
/* <40d86> ../engine/info.c:23 */
const char *Info_ValueForKey(const char *s, const char *key)
{
	// use few (two?) buffers so compares work without stomping on each other
	static char value[INFO_MAX_BUFFER_VALUES][MAX_KV_LEN];
	static int valueindex;
	char* dst;
	int nCount;
	int len;
	qboolean iskey = FALSE;

	len = Q_strlen(key);

	while (*s++ == '\\')
	{
		iskey ^= 1;

		if (iskey)
		{
			if (!Q_strncmp(s, key, len) && s[len] == '\\')
			{
				s += len + 1; // go to value
				dst = value[valueindex];

				for (nCount = 0; nCount < MAX_KV_LEN; nCount++)
				{
					if (*s == '\0' || *s == '\\')
						break;

					dst[nCount] = *s++;
				}

				dst[nCount] = '\0';
				valueindex = (valueindex + 1) % INFO_MAX_BUFFER_VALUES;
				return dst;
			}
		}

		// skip to next slash
		while (*s && *s != '\\')
			s++;
	}

	return "";
}

/* <40e38> ../engine/info.c:72 */
void Info_RemoveKey(char *s, const char *key)
{
	char *next;
	int len;
	qboolean iskey = FALSE;

	if (Q_strstr(key, "\\"))
	{
		Con_Printf("Can't use a key with a \\\n");
		return;
	}

	len = Q_strlen(key);

	while (*s++ == '\\')
	{
		iskey ^= 1;

		if (iskey)
		{
			if (!Q_strncmp(s, key, len) && s[len] == '\\')
			{
				next = Q_strchr(s + len + 1, '\\'); // go to end of value

				if (next)
					Q_memmove(s, next + 1, Q_strlen(next)); // '\' + len = len + '\0'
				else
					s[-1] = '\0'; // key is last

				break;
			}
		}

		// skip to next slash
		while (*s && *s != '\\')
			s++;
	}
}

/* <40ecf> ../engine/info.c:136 */
void Info_RemovePrefixedKeys(char *s, const char prefix)
{
	char *next;
	qboolean iskey = FALSE;

	while (*s++ == '\\')
	{
		iskey ^= 1;

		if (iskey)
		{
			if (s[0] == prefix)
			{
				next = Q_strchr(s + 1, '\\'); // skip key
				next = Q_strchr(next + 1, '\\'); // skip value

				if (next)
					Q_memmove(s, next + 1, Q_strlen(next)); // '\' + len = len + '\0'
				else
					s[-1] = '\0';

				s--; // back to slash for continue
				iskey = FALSE; // next field will be a key
				continue;
			}
		}

		// skip to next slash
		while (*s && *s != '\\')
			s++;
	}
}

/* <40d4a> ../engine/info.c:188 */
qboolean Info_IsKeyImportant(const char *key)
{
	if (key[0] == '*')
		return true;
	if (!Q_strcmp(key, "name"))
		return true;
	if (!Q_strcmp(key, "model"))
		return true;
	if (!Q_strcmp(key, "rate"))
		return true;
	if (!Q_strcmp(key, "topcolor"))
		return true;
	if (!Q_strcmp(key, "botomcolor"))
		return true;
	if (!Q_strcmp(key, "cl_updaterate"))
		return true;
	if (!Q_strcmp(key, "cl_lw"))
		return true;
	if (!Q_strcmp(key, "cl_lc"))
		return true;
#ifndef REHLDS_FIXES
	// keys starts from '*' already checked
	if (!Q_strcmp(key, "*hltv"))
		return true;
	if (!Q_strcmp(key, "*sid"))
		return true;
#endif

	return false;
}

/* <40f88> ../engine/info.c:216 */
char *Info_FindLargestKey(char *s, int maxsize)
{
	static char largest_key[MAX_KV_LEN];
	int largest_size = 0;
	const char* plargest = NULL;
	int len;
	char *next;
	qboolean iskey = FALSE;
	
	while (*s++ == '\\')
	{
		iskey ^= 1;

		if (iskey)
		{
			next = Q_strchr(s + 1, '\\'); // skip key
			len = next - s;
			*next = '\0';

			if (len > largest_size && !Info_IsKeyImportant(s))
			{
				largest_size = len;
				plargest = s;
			}

			*next = '\\';
			s = next;
			continue;
		}

		// skip to next slash
		while (*s && *s != '\\')
			s++;
	}

	if (largest_size)
	{
		Q_strncpy(largest_key, plargest, sizeof largest_key - 1);
		largest_key[sizeof largest_key - 1] = '\0';
		return largest_key;
	}

	return "";
}

/* <41063> ../engine/info.c:275 */
void Info_SetValueForStarKey(char *s, const char *key, const char *value, int maxsize)
{
	char newArray[MAX_INFO_STRING];

	if (!key || !value)
	{
		Con_Printf("Keys and values can't be null\n");
		return;
	}

	if (key[0] == 0)
	{
		Con_Printf("Keys can't be an empty string\n");
		return;
	}

	if (Q_strstr(key, "\\") || Q_strstr(value, "\\"))
	{
		Con_Printf("Can't use keys or values with a \\\n");
		return;
	}

	if (Q_strstr(key, "..") || Q_strstr(value, ".."))
	{
		Con_Printf("Can't use keys or values with a ..\n");
		return;
	}

	if (Q_strstr(key, "\"") || Q_strstr(value, "\""))
	{
		Con_Printf("Can't use keys or values with a \"\n");
		return;
	}

	int keyLen = Q_strlen(key);
	int valueLan = Q_strlen(value);

	if (keyLen >= MAX_KV_LEN || valueLan >= MAX_KV_LEN)
	{
		Con_Printf("Keys and values must be < %i characters\n", MAX_KV_LEN);
		return;
	}

	if (!Q_UnicodeValidate(key) || !Q_UnicodeValidate(value))
	{
		Con_Printf("Keys and values must be valid utf8 text\n");
		return;
	}

	// Remove current key/value and return if we doesn't specified to set a value
	Info_RemoveKey(s, key);
	if (value[0] == 0)
	{
		return;
	}

	// Create key/value pair
	int neededLength = Q_snprintf(newArray, MAX_INFO_STRING - 1, "\\%s\\%s", key, value);
	newArray[MAX_INFO_STRING - 1] = 0;

	if ((int)Q_strlen(s) + neededLength >= maxsize)
	{
		// no more room in the buffer to add key/value
		if (!Info_IsKeyImportant(key))
		{
			// no room to add setting
			Con_Printf("Info string length exceeded\n");
			return;
		}

		// keep removing the largest key/values until we have a room
		char *largekey;
		do
		{
			largekey = Info_FindLargestKey(s, maxsize);
			if (largekey[0] == 0)
			{
				// no room to add setting
				Con_Printf("Info string length exceeded\n");
				return;
			}
			Info_RemoveKey(s, largekey);
		} while ((int)Q_strlen(s) + neededLength >= maxsize);
	}

	// auto lowercase team
	if (!Q_stricmp(key, "team"))
	{
		strcat(s, "\\team\\");
		strcat(s, value);
	}
	else
		strcat(s, newArray);
}

/* <4113e> ../engine/info.c:361 */
void Info_SetValueForKey(char *s, const char *key, const char *value, int maxsize)
{
	if (key[0] == '*')
	{
		Con_Printf("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey(s, key, value, maxsize);
}

/* <41193> ../engine/info.c:372 */
void Info_Print(const char *s)
{
	const char *next;
	qboolean iskey = FALSE;
	char buf[1024];
	int len = 0;

	while (*s++ == '\\')
	{
		// print key
		next = Q_strchr(s + 1, '\\');
		len += Q_snprintf(buf + len, sizeof buf - len, "%-20.*s", next - s, s);
		s = next + 1;

		// print value
		next = Q_strchr(s + 1, '\\');
		if (next)
		{
			len += Q_snprintf(buf + len, sizeof buf - len, "%.*s\n", next - s, s);
			s = next;
		}
		else
		{
			// last value
			Q_snprintf(buf + len, sizeof buf - len, "%s\n", s);
			break;
		}
	}

	Con_Printf(buf);
}

/* <4120e> ../engine/info.c:426 */
qboolean Info_IsValid(const char *s)
{
	const char* v;

	while (*s++ == '\\')
	{
		// check key
		if (*s == '\\')
			return FALSE; // empty keys not allowed

		v = Q_strchr(s, '\\');

		if (v == NULL)
			return FALSE; // key should ends with a \, not a NULL

		if (v - s >= MAX_KV_LEN)
			return FALSE; // key length should be less then MAX_KV_LEN

		// check value
		v++;

		if (*v == '\\' || *v == '\0')
			return FALSE; // empty values not allowed

		s = Q_strchr(v, '\\');

		if (s == NULL)
			for (s = v; *s; s++) // last key, go to EOS
				;

		if (s - v >= MAX_KV_LEN)
			return FALSE; // key length should be less then MAX_KV_LEN
	}

	return TRUE;
}
