#ifndef __FLEX_UTIL_H_
#define __FLEX_UTIL_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern FILE* yyout;
#define OUT yyout

typedef int boolean;
#define DB(z) fprintf(stderr, "%s\n", z);
#define true 1
#define false 0

typedef struct Str_c {
    char* ptr;
    int len;

} Str_c;

static void STR_init(Str_c* str) {
    str->ptr = NULL;
    str->len = 0;
}

static Str_c STR_make(const char* text) {
    Str_c result;
    result.len = strlen(text);
    result.ptr = strdup(text);
    return result;
}

static Str_c STR_makeEmpty() {
    Str_c result;
    STR_init(&result);
    return result;
}

static void STR_append(Str_c* str, const char* text) {
    if (!str->len) {
        str->ptr = strdup(text);
        str->len = strlen(text);
        return;
    }
    str->len += strlen(text);
    str->ptr = (char*) realloc(str->ptr, (str->len + 1) * sizeof(char));
    strcat(str->ptr, text);

}

static void STR_addChar(Str_c* str, char c) {
    str->len += 1;
    str->ptr = (char*) realloc(str->ptr, (str->len + 1) * sizeof(char));
    str->ptr[str->len - 1] = c;
    str->ptr[str->len] = '\0';
}

// free the current string, init as empty
static void STR_clear(Str_c* str) {
    if (str->ptr) {
        free(str->ptr);
    }
    STR_init(str);
}
    
static void STR_appendAsUtf8 (Str_c* str, long ucode) {
    if (ucode < 0x80) {
        STR_addChar(str, ucode);
    } 
    else if (ucode < 0x800) {
        STR_addChar(str, (ucode >> 6)   | 0xC0);
        STR_addChar(str, (ucode & 0x3F) | 0x80);
    }
    else {
        STR_addChar(str, ((ucode >> 12)       ) | 0xE0);
        STR_addChar(str, ((ucode >> 6 ) & 0x3F) | 0x80);
        STR_addChar(str, ((ucode      ) & 0x3F) | 0x80);
    }
}

static void printMultiple(char c, int times, FILE* fp) {
    int i = 0;
    for (i = 0; i < times; ++i) {
        fputc(c, fp);
    }
}

#define VecDefine(NAME, ADDNAME, VEC_TYPE)  \
typedef struct NAME {  \
    VEC_TYPE* data; \
    unsigned int size;   \
    unsigned int slack; \
} NAME; \
static void ADDNAME(NAME* v, VEC_TYPE elem) { \
    if (v->slack == 0) {   \
        v->data = (VEC_TYPE*) malloc(10 * sizeof(VEC_TYPE));   \
        v->size = 0;    \
        v->slack = 10; \
    }   \
    if (v->size + 1 >= v->slack) { \
        v->slack += v->slack; \
        v->data = (VEC_TYPE*) realloc(v->data, v->slack * sizeof(VEC_TYPE)); \
    }   \
    v->data[v->size++] = elem; \
}

VecDefine(Vec_LongLong, VLL_add, long long);
VecDefine(Vec_Str, VS_add, Str_c);

// Holds parse state, used for reporting errors.
typedef struct ParserState {
    // The line number we are currently parsing.
    int LineNum;

    // The contents of the everything we have parsed so far, by line.
    Vec_Str LineTexts;

    // The last token flex parsed.
    Str_c LastMatch;
   
} ParserState;

// Due to the way each file from flex & bison partialy includes one another we make this extern and define it in the .l
extern ParserState parser;

static void PS_Init() {
    parser.LineNum = 0;
    parser.LineTexts.slack = 0;
    VS_add(&parser.LineTexts, STR_makeEmpty());
}

static void PS_Free() {
    free(parser.LineTexts.data);
    STR_clear(&parser.LastMatch);
}

// Called always when there is a flex rule match
static void PS_Match(char* text) {
    STR_append(&parser.LineTexts.data[parser.LineNum], text);
    STR_clear(&parser.LastMatch);
    STR_append(&parser.LastMatch, text);
}

// Once we found a \n
static void PS_CountLine() {
    ++parser.LineNum;
    VS_add(&parser.LineTexts, STR_makeEmpty());
}

// Prints a "pretty" formatted line
static int PS_PrintLine(int Index) {
    if (Index < 0) {
        return 0;
    }
    fprintf(stderr, "Line %3.d: %s\n", Index, parser.LineTexts.data[Index].ptr);
    return parser.LineTexts.data[Index].len;
}

// Prints a "pretty" fromatted error including the previous line for context.
static void PS_ReportErrorAtOffset(int offset) {
    if (parser.LineTexts.data[parser.LineNum].len > 0) {
        int start_index = parser.LineTexts.data[parser.LineNum].len - offset;
        if (start_index < 0) {
            start_index = 0;
        }
        fprintf(stderr, "Failed to parse: '%s'\n", (parser.LineTexts.data[parser.LineNum].ptr + start_index));
    }

    PS_PrintLine(parser.LineNum - 1);
    int error_loc = PS_PrintLine(parser.LineNum) - offset;

    printMultiple('>', 9, stderr);
    printMultiple('-', error_loc, stderr);
    fputc(' ', stderr);
    printMultiple('^', parser.LastMatch.len, stderr);
    fputc('\n', stderr);
}

static void PS_ReportLastTokenError() {
        if (parser.LastMatch.len) {
            PS_ReportErrorAtOffset(parser.LastMatch.len);
        }
    }

// Called directly from yyerror() and prints the last 2 lines parsed with the last
// matched token underlined
static void PS_ReportError(const char* reason) {
    PS_ReportLastTokenError();
    fprintf(stderr, "Reason: %s\n", reason);
}

static char* util_MakeString(char* from) {
    return strndup(from + 1, strlen(from) - 2);
}

static float util_MakeFloat(char* from) {
    return atof(from);
}

static long long util_MakeInt(char* from) {
    return atoll(from);
}

struct JObject;
struct JArray;
struct JString;

// Global DB keeping track of ids
typedef struct JsonDB {
    Vec_Str IdStrs;
    Vec_LongLong UserIds;
} JsonDB;

extern JsonDB database;
// Attempts to Insert an id_str element in the database. Returns false if it already existed
static boolean MaybeInsertIdStr(const char* id_str);
// Attempts to Insert a user_id element in the database. Returns false if it already existed
static boolean MaybeInsertUserId(long long id);


enum JValueType {
    E_Object,
    E_Array,
    E_String,
    E_Float,
    E_Int,
    E_Bool,
    E_NullVal
};

struct JObject;
struct JArray;
struct JString;

union JValueData {
    struct JObject* ObjectData;
    struct JArray* ArrayData;
    struct JString* StringData;
    float FloatData;
    long long IntData;
    boolean BoolData;
};

// Special members are all the members required for the assignment.
enum JSpecialMember {
    E_None, // Not a special member
    E_IdStr,
    E_Text,
    E_CreatedAt,
    E_User,
    E_UName,
    E_UScreenName,
    E_ULocation,
    E_UId,
    // Assignment 2a
    E_TweetObj,
    // Assignment 2b
    E_ExTweet,
    E_Truncated,
    E_DisplayRange,
    E_Entities,
    E_Hashtags,
    E_Indices,
    E_FullText
};

// POD utility for storing a starting point of a hashtag and its text.
typedef struct HashTagData {
    Str_c Tag;
    unsigned int Begin;
} HashTagData;
VecDefine(Vec_Hashtags, VH_add, HashTagData);

// We use our own specialized string struct that stores hash tags, length, byte length.
typedef struct JString {
    // 'actuall' length after merging unicode characters as 1 chars.
    // to get byte length use Text.length()
    unsigned int Length;

    // Converted text.
    Str_c Text;
    // This will contain the hashtags found (if any)
    Vec_Hashtags Hashtags;
    Str_c RetweetUser;
} JString;

static JString* Alloc_JString(char* cstring);

typedef struct JValue {
    enum JValueType Type;
    union JValueData Data;
} JValue;
static void Print_JValue(JValue* v, int indentation);

#define DECLARE_VAL_ALLOC(DECLAR, VALUE_TYPE, DATA_ASSIGNMENT) static JValue* DECLAR { \
    JValue* v = (JValue *) malloc(sizeof(JValue));  \
    v->Type = VALUE_TYPE;   \
    v->Data.DATA_ASSIGNMENT; \
    return v;   \
}

DECLARE_VAL_ALLOC(JV_Null  ()                   , E_NullVal , ObjectData = NULL);
DECLARE_VAL_ALLOC(JV_Object(struct JObject* obj), E_Object  , ObjectData = obj );
DECLARE_VAL_ALLOC(JV_Array (struct JArray* a)   , E_Array   , ArrayData  = a   );
DECLARE_VAL_ALLOC(JV_String(char* s)            , E_String  , StringData = Alloc_JString(s));
DECLARE_VAL_ALLOC(JV_StrObj(struct JString* s)  , E_String  , StringData = s   );
DECLARE_VAL_ALLOC(JV_Float (float num)          , E_Float   , FloatData  = num );
DECLARE_VAL_ALLOC(JV_Int   (long long num)      , E_Int     , IntData    = num );
DECLARE_VAL_ALLOC(JV_Bool  (int val)            , E_Bool    , BoolData   = val );

#undef DECLARE_VAL_ALLOC

// Utility for ranges: arrays with 2 ints
typedef struct JRange {
    long long Begin;
    long long End;
} JRange;

VecDefine(Vec_JValuePtr, VVP_add, JValue*);
typedef struct JArray {
    Vec_JValuePtr Elements;
    JRange AsRange;

    // this is only used if this is a hashtag array.
    Vec_Hashtags Hashtags;

} JArray;
static void Print_JArray(JArray* a, int indentation);

// Attempts to exract and populate the Hashtags vector from the elements.
// Returns true if this array forms a valid "hashtags" array. 
static boolean ExtractHashtags(JArray* a, Str_c* Error);

static JArray* Alloc_JArray() {
    JArray* a;
    a = (JArray*) malloc(sizeof(JArray));
    a->Hashtags.slack = 0;
    a->Elements.slack = 0;

    a->AsRange.Begin = -1;
    a->AsRange.End = -1;
    return a;
}

static JArray* Alloc_JArrayRange(long long from, long long to) {
    JArray* a;
    a = (JArray*) malloc(sizeof(JArray));
    a->Hashtags.slack = 0;
    a->Elements.slack = 0;
    
    a->AsRange.Begin = from;
    a->AsRange.End = to;
    
    // Watch out the order here...
    // We 'emulate' our parsing and push back in reverse order.
    VVP_add(&a->Elements, JV_Int(to));
    VVP_add(&a->Elements, JV_Int(from));

    return a;
}

typedef struct JMember {
    Str_c Name;
    JValue* Value;
    enum JSpecialMember SpecialType;
} JMember;

static JMember* Alloc_JMember(const char* name, JValue* value, enum JSpecialMember type) {
    JMember* m;
    m = (JMember*) malloc(sizeof(JMember));

    m->Name = STR_make(name);
    m->Value = value;
    m->SpecialType = type;
    return m;
}

// A 'special' members struct that holds pointers to specific assignment dependant members
// We don't save the actual metadata but pointers to the values directly
// when any of these pointers are null it means the object does not contain that specific member at all
//
// eg: we only care about the actual JString of a member with name 'text' and we only accept JString as its value.
typedef struct JSpecialMembers {
    JString* IdStr;
    JString* Text;
    JString* CreatedAt;
    struct JObject* User;

    JString* UName;
    JString* UScreenName;
    JString* ULocation;
    long long* UId;

    struct JObject* TweetObj;
} JSpecialMembers;

// Special members for extended tweets, same as above
typedef struct JExSpecialMembers {
    struct JObject* ExTweet;
    boolean* Truncated;
    JRange* DisplayRange;
    struct JObject* Entities;
    JArray* Hashtags;
    JRange* Indices;
    JString* FullText;
} JExSpecialMembers;

VecDefine(Vec_JMemberPtr, VMP_add, JMember*);

typedef struct JObject {
    Vec_JMemberPtr Memberlist;
    JSpecialMembers Members;
    JExSpecialMembers ExMembers;

} JObject;
static void Print_JObject(JObject* o, int indentation);

// Add a member to the Memberlist and resolve if it needs to popule some Members.* or ExMembers.* field.
static void AddMemberToObject(JObject* o, JMember* member);

// Checks if this JObject forms a valid "outer" object 
// ie MUST have text, valid user, IdStr, date AND extra if truncated = true
static boolean FormsValidOuterObject(JObject* o, Str_c* FailMessage);

// Checks if this JObject forms a valid "extended tweet" object
// extended tweet MUST include valid hashtags as entities if there are any.
static boolean FormsValidExtendedTweetObj(JObject* o, Str_c* FailMessage);

static JObject* Alloc_JObject() {
    JObject* o;
    o = (JObject*) malloc(sizeof(JObject));

    o->Memberlist.slack = 0;

    o->Members.IdStr = NULL;
    o->Members.Text = NULL;
    o->Members.CreatedAt = NULL;
    o->Members.User = NULL;
    o->Members.UName = NULL;
    o->Members.UScreenName = NULL;
    o->Members.ULocation = NULL;
    o->Members.UId = NULL;
    o->Members.TweetObj = NULL;

    o->ExMembers.ExTweet = NULL;
    o->ExMembers.Truncated = NULL;
    o->ExMembers.DisplayRange = NULL;
    o->ExMembers.Entities = NULL;
    o->ExMembers.Hashtags = NULL;
    o->ExMembers.Indices = NULL;
    o->ExMembers.FullText = NULL;
    return o;
}

static void Indent(int num) {
    printMultiple(' ', num * 2, OUT);
}

static boolean MaybeInsertIdStr(const char* data) {
    for (int i = 0; i < database.IdStrs.size; ++i) {
        if (strcmp(data, database.IdStrs.data[i].ptr) == 0) {
            return false;
        }
    }
    VS_add(&database.IdStrs, STR_make(data));
    return true;
}

static boolean MaybeInsertUserId(long long id) {
    for (int i = 0; i < database.UserIds.size; ++i) {
        if (database.UserIds.data[i] == id) {
            return false;
        }
    }
    VLL_add(&database.UserIds, id);
    return true;
}

static void Print_JValue(JValue* v, int indent) {
    switch(v->Type) {
        case E_Object:
            Print_JObject(v->Data.ObjectData, indent);
            break;
        case E_Array:
            Print_JArray(v->Data.ArrayData, indent);
            break;
        case E_String:
            fprintf(OUT, "\"%s\"", v->Data.StringData->Text.ptr);
            break;
        case E_Float:
            fprintf(OUT, "%f", v->Data.FloatData);
            break;
        case E_Int:
            fprintf(OUT, "%lli", v->Data.IntData);
            break;
        case E_Bool:
            if (v->Data.BoolData) {
                fprintf(OUT, "true");
            }
            else {
                fprintf(OUT, "false");
            }
            break;
        case E_NullVal:
            fprintf(OUT, "null");
            break;
    }
}

static void Print_JArray(JArray* a, int indent) {
    fprintf(OUT, "[ ");      // Print [ to OUT
    int i = 0;
    for (i = 0; i < a->Elements.size; ++i) { // for each element as el
        fprintf(OUT, "\n");
        Indent(indent + 1);    // add (indent + 1) * \t tab characters
        Print_JValue(a->Elements.data[i], indent + 1); // call print for value at indent + 1
        fprintf(OUT, ",");
    }
    fprintf(OUT, "\b \n"); // backspace last comma
    Indent(indent);
    fprintf(OUT, "]"); 
}

static void Print_JObject(JObject* o, int indent) {
    fprintf(OUT, "{ "); 
    int i = 0;
    for (i = o->Memberlist.size - 1; i >= 0; --i) { // reverse of push_back'ed order.
        fprintf(OUT, "\n"); 
        Indent(indent + 1);
        fprintf(OUT, "\"%s\": ", o->Memberlist.data[i]->Name.ptr);
        Print_JValue(o->Memberlist.data[i]->Value, indent + 1);
        fprintf(OUT, ","); 
    }
    fprintf(OUT, "\b \n");
    Indent(indent);
    fprintf(OUT, "}"); 
}

static JString* Alloc_JString(char* source) {
    JString* s = (JString *)malloc(sizeof(JString));

    s->RetweetUser = STR_makeEmpty();
    s->Text = STR_makeEmpty();
    s->Hashtags.slack = 0;
    const int sourcelen = strlen(source);

    s->Length = 0;
    for (int i = 0; i < sourcelen; ++i) {
        char c = source[i];
        
        if (c == '\\') {
            // This will never overflow beause we only allow valid strings through flex.
            char esc = source[i+1];

            switch (esc) {
                case 'n': {
                    STR_addChar(&s->Text, '\n');
                    s->Length++;
                    i++;
                    break;
                }
                case 'u': {
                    // we are sure that this will always read the correct number of bytes because we check for this in our lexer.
                    // explicit type ensures we have enough width and correct representation for the conversion
                    long u_code = strtol(&source[i+2], NULL, 16);
                    STR_appendAsUtf8(&s->Text, u_code);

                    s->Length++; // Count this as 1 'actual' character
                    i += 5;   // increase by characters read after \ (eg: u2345)
                    break;
                }
                default: {
                    // All other cases just push the character that was escaped.
                    STR_addChar(&s->Text, esc);
                    s->Length++;
                    i++;
                    break;
                }
            }
        }
        else if (c == '%') {
            // Care here to not go out of bounds, its possible to have "... %\0"
            // it is assumed that what does not match our 5 special characters gets added as literal text.
            boolean FoundMatch = false;
            
            if (source[i+1] == '2') {
                // source[i+2] is not out of bounds here. (it can be '\0')
                switch(source[i+2]) {
                    case 'B': {
                        STR_addChar(&s->Text, '+');
                        s->Length++;
                        i += 2;
                        FoundMatch = true;
                        break;
                    }
                    case '1': {
                        STR_addChar(&s->Text, '!');
                        s->Length++;
                        i += 2;
                        FoundMatch = true;
                        break;
                    }
                    case '0': {
                        STR_addChar(&s->Text, ' ');
                        s->Length++;
                        i += 2;
                        FoundMatch = true;
                        break;
                    }
                    case 'C': {
                        STR_addChar(&s->Text, ',');
                        s->Length++;
                        i += 2;
                        FoundMatch = true;
                        break;
                    }
                    case '6': {
                        STR_addChar(&s->Text, '&');
                        s->Length++;
                        i += 2;
                        FoundMatch = true;
                        break;
                    }
                }
            }

            // if no match found just add '%'
            if (!FoundMatch) {
                STR_addChar(&s->Text, c);
                s->Length++;
            }
        }
        else if (c == '#') {
            // Hashtag begins here.
            HashTagData hashtag;
            hashtag.Tag = STR_makeEmpty();
            char *readptr = &source[i+1];
            while((*readptr >= 'a' && *readptr <= 'z') || // Allow a-z
                  (*readptr >= 'A' && *readptr <= 'Z') || // Allow A-Z
                  (*readptr >= '0' && *readptr <= '9') || // Allow 0-9
                  *readptr == '_')  // Allow underscore
            {                                           // Everything else stops the hashtag
                STR_addChar(&hashtag.Tag, *readptr);
                readptr++;
            }
            
            if (hashtag.Tag.len > 0) {
                // We have a valid hashtag.
                // Assumes indices count escaped sequences as 1 character. (eg: "text"="/u2330 #abc" starts at 2)
                hashtag.Begin = s->Length; // conatins the '#' param
                VH_add(&s->Hashtags, hashtag);
            } // the rest of the code works both for empty or non-empty TempHashtag

            STR_addChar(&s->Text, '#');
            STR_append(&s->Text, hashtag.Tag.ptr);

            s->Length += hashtag.Tag.len + 1; // Count unicode formatted characters added to text.

            i += hashtag.Tag.len; // Forward by the length of the hashtag
        }
        else {
            STR_addChar(&s->Text, c);
            s->Length++;
        }
    }

    // finally extract RT @user from the string if exists
    if (source[0] == 'R' &&
        source[1] == 'T' &&
        source[2] == ' ' &&
        source[3] == '@') 
    {
        char *readptr = &source[4];
        while(  (*readptr > 'a' && *readptr < 'z') || // Allow a-z
                (*readptr > 'A' && *readptr < 'Z') || // Allow A-Z
                (*readptr > '0' && *readptr < '9') || // Allow 0-9
                 *readptr == '_')  // Allow underscore
        {
            STR_addChar(&s->RetweetUser, *readptr);
            readptr++;
        }
    }
    return s;
}

static void AddMemberToObject(JObject* o, JMember* member) {
    // When adding a member if it is special we populate the specific Object Field with its data.
    // the final (simplified) JObject outline looks something like this after we are done adding members.
    // JObject 
    //    Members[]           - A reversed list with all the members (used for printing output)
    //    ...
    //    IdStr = nullptr     - field 'IdStr' is not present in this object
    //    Text = JString*     - value of 'text' json field
    //    User = JObject*     - value of 'user' json field
    //    
    VMP_add(&o->Memberlist, member);
    switch(member->SpecialType) {
        case E_IdStr:         o->Members.IdStr       = member->Value->Data.StringData; break;
        case E_Text:          o->Members.Text        = member->Value->Data.StringData; break;
        case E_CreatedAt:     o->Members.CreatedAt   = member->Value->Data.StringData; break;
        case E_User:          o->Members.User        = member->Value->Data.ObjectData; break;
        case E_UName:         o->Members.UName       = member->Value->Data.StringData; break;
        case E_UScreenName:   o->Members.UScreenName = member->Value->Data.StringData; break;
        case E_ULocation:     o->Members.ULocation   = member->Value->Data.StringData; break;
        case E_UId:           o->Members.UId         = &member->Value->Data.IntData; break;
        case E_TweetObj:      o->Members.TweetObj    = member->Value->Data.ObjectData; break;
        case E_ExTweet:       o->ExMembers.ExTweet       = member->Value->Data.ObjectData; break;
        case E_Truncated:     o->ExMembers.Truncated     = &member->Value->Data.BoolData; break;
        case E_DisplayRange:  o->ExMembers.DisplayRange  = &member->Value->Data.ArrayData->AsRange; break;
        case E_Entities:      o->ExMembers.Entities      = member->Value->Data.ObjectData; break;
        case E_Hashtags:      o->ExMembers.Hashtags      = member->Value->Data.ArrayData; break;
        case E_Indices:       o->ExMembers.Indices       = &member->Value->Data.ArrayData->AsRange; break;
        case E_FullText:      o->ExMembers.FullText      = member->Value->Data.StringData; break;
    }
}

static boolean FormsValidOuterObject(JObject* o, Str_c* FailMessage) {
    
    // Our outer object MUST include IdStr, Text, User, Date.
    if (!o->Members.IdStr || !o->Members.Text || !o->Members.User || !o->Members.CreatedAt) {
        STR_append(FailMessage, "Missing field IdStr/Text/User/CreatedAt");
        return false;
    }

    // if we dont find a truncated value or found one that == false we are done.
    if (!o->ExMembers.Truncated || *o->ExMembers.Truncated == false) {
        return true;
    }

    // Otherwise truncated = true so the object MUST include:
    // * a valid display_text_range [0, Members.Text.Length]
    // * an exteneded tweet object 

    if (!o->ExMembers.DisplayRange) {
        STR_append(FailMessage, "Missing display range when truncated == true.");
        return false;
    }

    // The display range must be from x->x+Length and x+Length must be less than 
    unsigned int TextSize = o->Members.Text->Length;
    if (o->ExMembers.DisplayRange->Begin != 0 ||
        o->ExMembers.DisplayRange->End != TextSize) {

        STR_append(FailMessage, "Display Range did not match the given text.");
        return false;
    }

    // Finally check for the extended tweet object. 
    if (!o->ExMembers.ExTweet) {
        STR_append(FailMessage, "Missing extended tweet when truncated == true.");
        return false;
    }

    // We found an ExTweet. 
    // This cannot be invalid since it has been already checked it was first parsed.
    
    // All required fields have been found and validated.
    return true;
}

static boolean FormsValidExtendedTweetObj(JObject* o, Str_c* FailMessage) {

    // Require full_text and display range.
    if (!o->ExMembers.FullText || !o->ExMembers.DisplayRange) {
        STR_append(FailMessage, "Missing 'full_text' and/or 'display_text_range'.");
        return false;
    }

    // Validate text length with display range
    unsigned int TextSize = o->ExMembers.FullText->Length;
    if (o->ExMembers.DisplayRange->Begin != 0 ||
        o->ExMembers.DisplayRange->End != TextSize) {

        char Message[200];
        sprintf(Message, 
            "Display Range did not match the given full_text.\nText Size: %d DisplayRange: [%lli, %lli]",
            TextSize, o->ExMembers.DisplayRange->Begin, o->ExMembers.DisplayRange->End
        );
        STR_append(FailMessage, Message);
        return false;
    }
    
    JString* TextObj = o->ExMembers.FullText;

    // Even if we have NO hashtags in the text, we still need to verify that
    // there are no recorded hashtags in the array (but only if one exists.)

    int HashtagsInArray = 0;
    if (o->ExMembers.Entities) {
        HashtagsInArray = o->ExMembers.Entities->ExMembers.Hashtags->Hashtags.size;
    }

    if (HashtagsInArray != TextObj->Hashtags.size) {
        STR_append(FailMessage, "Hashtags found in text did not match all the hashtags in the entites.");
        return false;
    }

    // Hashtag count was equal. If it is also 0 then this is a valid extended_tweet
    if (HashtagsInArray == 0) {
        return true;
    }

    // The hashtags found in the entities array.
    Vec_Hashtags* EntitiesTags = &o->ExMembers.Entities->ExMembers.Hashtags->Hashtags;

    // All that is left is to verify hashtag positions on the actual text.
    // The hash tags could be in random order so do N^2 for now. 
    int i = 0;
    int j = 0;
    for (i = 0; i < TextObj->Hashtags.size; ++i) {
        const HashTagData* Outer = &TextObj->Hashtags.data[i];
        int found = 0;
        for (j = 0; j < EntitiesTags->size; ++j) {
            HashTagData* other = &EntitiesTags->data[j];  
            
            int equal = 
                Outer->Begin == other->Begin && 
                (strcmp(Outer->Tag.ptr, other->Tag.ptr) == 0);

            if (equal) {
                found = 1;
                break;
            }
        }

        if (!found) {
            char AddedStr[200];
            sprintf(AddedStr, "Hashtag: '%s' is missing from the entities array or has incorrect Indices.", Outer->Tag.ptr);
            STR_append(FailMessage, AddedStr);
            return false;
        }
    }

    return true;
}

static boolean ExtractHashtags(JArray* a, Str_c* Error) {
    // This array must ONLY include objects that contain 'text', 'indices'
    // the array can be empty.
    // Actual checking for hashtag locations cannot be performed at this stage
    
    int i = 0;
    for (i = 0; i < a->Elements.size; ++i) {
        JValue* Element = a->Elements.data[i];
        // Must be an object type
        if (Element->Type != E_Object) {
            STR_append(Error, "An element of the array is not an object.");
            return false;
        }

        // Now check this object to find if it includes "text" & "indices" they are required.
        JObject* Subobject = Element->Data.ObjectData;
        if (!Subobject->Members.Text || !Subobject->ExMembers.Indices) {
            STR_append(Error, "An element of the array is missing 'text' and/or 'indices'.");
            return false;
        }

        // Also validate indice length. The actual # is not included in the text but it is accounted in indice length
        // Therefore we need to offset the length by 1
        long long IndiceLength = Subobject->ExMembers.Indices->End - Subobject->ExMembers.Indices->Begin;
        if (IndiceLength != Subobject->Members.Text->Length + 1) {
            STR_append(Error, "Indice range did not match the text length.");
            return false;

        }

        // Everything is correct. Collect this result and add it to the hashtags vector.
        HashTagData Data;
        Data.Tag = STR_make(Subobject->Members.Text->Text.ptr);
        Data.Begin = Subobject->ExMembers.Indices->Begin;

        VH_add(&a->Hashtags, Data);
    }
    return true;
}


#endif //__FLEX_UTIL_H_