

#ifndef _BANK_H_
#define _BANK_H_

#define BANK_TYPE_EPS		0x00
#define BANK_TYPE_EPS16		0x01
#define BANK_TYPE_ASR		0x02
#define BANK_TYPE_UNKNOWN 	0x03


//----------------------------------------------------------------------------
// list of copied banks
//----------------------------------------------------------------------------
typedef struct _POST_PROCESS_ITEM
{
	char cMsDosName[260];	// device name starting with "\\.\" or image file
	struct _POST_PROCESS_ITEM *pNext;	// pointer to next entry
} POST_PROCESS_ITEM;

typedef struct _ITEM_INDEX_LIST
{
	unsigned char index[12]; // maximum depth is 12 levels
} ITEM_INDEX_LIST;

typedef struct _INDEX_LIST_PAIR
{
	ITEM_INDEX_LIST SourceIndexList;
	ITEM_INDEX_LIST TargetIndexList;
	struct _INDEX_LIST_PAIR *pNext;
} INDEX_LIST_PAIR;


// prototypes
int isBankFile( unsigned char type );
int GetIndexListFromPath(FIND_HANDLE *pHandle, ITEM_INDEX_LIST *pIndexList);
void appendIndex( ITEM_INDEX_LIST *pIndexListItem, unsigned char indexEntry );
INDEX_LIST_PAIR *createIndexListTranslationItem();
void addItemToPostProcess( char* cMsDosName );
void freePostProcessItems( void );
void freeIndexListTranslation( void );
void doPostProcess( void );

#endif
