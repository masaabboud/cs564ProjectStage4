#include "heapfile.h"
#include "error.h"

// routine to create a heapfile
const Status createHeapFile(const string fileName)
{
   File*       file;
    Status      status;
    FileHdrPage*    hdrPage;
    int         hdrPageNo;
    int         newPageNo;
    Page*       newPage;
    //added this ptr tp use allocPage, may get rd of 
    Page*       page;

    // try to open the file. This should return an error (file is still unitilized)
    status = db.openFile(fileName, file);
    if(status == OK)
    {
        return FILEEXISTS;
    }
    if (status != OK)
    {
        // create file which makes the file exist
        status = db.createFile(fileName);
        // call open file again to actually initilize the pointer
        status = db.openFile(fileName, file);
        //alloc page
        status = bufMgr -> allocPage(file, hdrPageNo, page);
        hdrPage = (FileHdrPage*) page;

        //initilize values in hdrPage
        hdrPage -> firstPage = -1;
        hdrPage -> lastPage = -1;
        hdrPage -> pageCnt = 1;
        hdrPage -> recCnt = 0;
        
        //allocte data page
        status = bufMgr-> allocPage(file, newPageNo, newPage);
        newPage -> init(newPageNo);

        //update headerpage
        hdrPage->firstPage = newPageNo;
        hdrPage->lastPage = newPageNo;

        status = bufMgr -> unPinPage(file, hdrPageNo, true);
        if (status != OK) cerr << "error in unpin of header page\n";

        status = bufMgr -> unPinPage(file, newPageNo, true);
        if (status != OK) cerr << "error in unpin of data page\n";      
    }
    return OK;
}



// routine to destroy a heapfile
const Status destroyHeapFile(const string fileName)
{
	return (db.destroyFile (fileName));
}

// constructor opens the underlying file
HeapFile::HeapFile(const string & fileName, Status& returnStatus)
{
    Status  status;
    Page*   pagePtr;

    cout << "opening file " << fileName << endl;

    // open the file and read in the header page and the first data page
    if ((status = db.openFile(fileName, filePtr)) == OK)
    {
    //Next, it reads and pins the header page for the file in the buffer pool, initializing the private data members headerPage, headerPageNo, and hdrDirtyFlag.
    int hdrPageNo;
    status = filePtr->getFirstPage(hdrPageNo);
    status = bufMgr->readPage(filePtr, hdrPageNo, pagePtr); //also pins it??

    //initializing the private data members headerPage, headerPageNo, and hdrDirtyFlag.
    //int hdrPageNo;
    headerPage = (FileHdrPage*) pagePtr;
    headerPageNo = hdrPageNo;
    hdrDirtyFlag = false;  //is this initially false?

    //Finally, read and pin the first page of the file into the buffer pool, initializing the values of curPage, curPageNo, and curDirtyFlag appropriately
    curPageNo = headerPage->firstPage;
    bufMgr->readPage(filePtr, curPageNo, pagePtr);
    curPage = pagePtr;
    curDirtyFlag = false;

    //Set curRec to NULLRID 
    curRec = NULLRID;

    }
    else
    {
        cerr << "open of heap file failed\n";
        returnStatus = status;
        return;
    }
}



// the destructor closes the file
HeapFile::~HeapFile()
{
    Status status;
    cout << "invoking heapfile destructor on file " << headerPage->fileName << endl;

    // see if there is a pinned data page. If so, unpin it 
    if (curPage != NULL)
    {
    	status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
		curPage = NULL;
		curPageNo = 0;
		curDirtyFlag = false;
		if (status != OK) cerr << "error in unpin of date page\n";
    }
	
	 // unpin the header page
    status = bufMgr->unPinPage(filePtr, headerPageNo, hdrDirtyFlag);
    if (status != OK) cerr << "error in unpin of header page\n";
	
	// status = bufMgr->flushFile(filePtr);  // make sure all pages of the file are flushed to disk
	// if (status != OK) cerr << "error in flushFile call\n";
	// before close the file
	status = db.closeFile(filePtr);
    if (status != OK)
    {
		cerr << "error in closefile call\n";
		Error e;
		e.print (status);
    }
}

// Return number of records in heap file

const int HeapFile::getRecCnt() const
{
  return headerPage->recCnt;
}

//retrieve an arbitrary record from a file.
// if record is not on the currently pinned page
    // current page is unpinned
    //the required page is read into the buffer pooland pinned
    //returns a pointer to the record via the rec parameter

const Status HeapFile::getRecord(const RID & rid, Record & rec)
{
    Status status;

    // cout<< "getRecord. record (" << rid.pageNo << "." << rid.slotNo << ")" << endl;

    //Kavya addition start

   //Page* curPage; : data page currently pinned in buffer pool
   //int   curPageNo; : page number of pinned page

    //record is not on currently pinned page
    cout<< "getRecord. record (" << rid.pageNo << "." << rid.slotNo << ")" << endl;
    cout<< "getRecord. record (" << curPageNo << "." << rid.slotNo << ")" << endl;
    if (rid.pageNo != curPageNo) {
        //unpin curPageNo - is this necessary? do we have to unpin or can both be pinned simeltaniously?
        //status = bufMgr->unPinPage(filePtr, curPageNo, false);
        //read into buffer and pin page (readPage pins the page)
        status = bufMgr->readPage(filePtr, rid.pageNo, curPage);
        cout<< "getRecord. record (" << curPageNo << "." << rid.slotNo << ")" << endl;
        //Update the current page number to reflect the new page - is this necessary?
        curPageNo = rid.pageNo;
    }


    //we know current page has the correct pointer
    //this "getRecord" is from the page class
    Status rstatus = curPage->getRecord(rid, rec);
    return rstatus;
    
    //Kavya addition end
}

HeapFileScan::HeapFileScan(const string & name,
			   Status & status) : HeapFile(name, status)
{
    filter = NULL;
}

const Status HeapFileScan::startScan(const int offset_,
				     const int length_,
				     const Datatype type_, 
				     const char* filter_,
				     const Operator op_)
{
    if (!filter_) {                        // no filtering requested
        filter = NULL;
        return OK;
    }
    
    if ((offset_ < 0 || length_ < 1) ||
        (type_ != STRING && type_ != INTEGER && type_ != FLOAT) ||
        (type_ == INTEGER && length_ != sizeof(int)
         || type_ == FLOAT && length_ != sizeof(float)) ||
        (op_ != LT && op_ != LTE && op_ != EQ && op_ != GTE && op_ != GT && op_ != NE))
    {
        return BADSCANPARM;
    }

    offset = offset_;
    length = length_;
    type = type_;
    filter = filter_;
    op = op_;

    return OK;
}


const Status HeapFileScan::endScan()
{
    Status status;
    // generally must unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        curPage = NULL;
        curPageNo = 0;
		curDirtyFlag = false;
        return status;
    }
    return OK;
}

HeapFileScan::~HeapFileScan()
{
    endScan();
}

const Status HeapFileScan::markScan()
{
    // make a snapshot of the state of the scan
    markedPageNo = curPageNo;
    markedRec = curRec;
    return OK;
}

const Status HeapFileScan::resetScan()
{
    Status status;
    if (markedPageNo != curPageNo) 
    {
		if (curPage != NULL)
		{
			status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
			if (status != OK) return status;
		}
		// restore curPageNo and curRec values
		curPageNo = markedPageNo;
		curRec = markedRec;
		// then read the page
		status = bufMgr->readPage(filePtr, curPageNo, curPage);
		if (status != OK) return status;
		curDirtyFlag = false; // it will be clean
    }
    else curRec = markedRec;
    return OK;
}

//returns the RID of the next record that satisfies the scan predicate
const Status HeapFileScan::scanNext(RID& outRid)
{
    Status    status = OK;
    RID     nextRid;
    RID     tmpRid;
    int     nextPageNo;
    Record      rec; 
    Page* page;

    //We want to loop through the pages in the file
    while(status == OK){
        //keep the current page pinned until all the records on the page have been processed
        //nextRID isnt sewt to anythig?
        nextPageNo = nextRid.pageNo;
        status = bufMgr -> readPage(filePtr, nextPageNo, page);
       // page->pinCnt = 1;
        //if we cant read status most likely there is not a next page
        if(status != OK)
        {
        return status;
        }
        //now that we are on a page, we need to access the records on the page

        tmpRid = nextRid;
        Status recordStatus = page -> firstRecord(tmpRid);

         //we need to loop through the records on a page and check 
        //if they match requirements to be returned
         while (recordStatus == OK) //if we cant get a record, it probs ran out
        {
            /*
                For each page, use the firstRecord() and nextRecord()
                methods of the Page class to get the rids of all the records 
                on the page.
            */
            //const Status nextRecord(const RID& curRid, RID& nextRid) const;
            page->getRecord(tmpRid, rec);
            status = page -> nextRecord(tmpRid, nextRid);

            *RID temp = (*RID) tmpRid;
            // const bool matchRec(const Record & rec) const;
            temp -> matchRec(rec);
            /*
            Convert the rid to a pointer to the record data and 
            invoke matchRec() to determine if record satisfies the filter 
            associated with the scan.
            */
            
        }

    }
    
    //page->pinCnt = 0;
    return OK;
        
}



// returns pointer to the current record.  page is left pinned
// and the scan logic is required to unpin the page 

const Status HeapFileScan::getRecord(Record & rec)
{
    
    return curPage->getRecord(curRec, rec);
}

// delete record from file. 
const Status HeapFileScan::deleteRecord()
{
    Status status;

    // delete the "current" record from the page
    status = curPage->deleteRecord(curRec);
    curDirtyFlag = true;

    // reduce count of number of records in the file
    headerPage->recCnt--;
    hdrDirtyFlag = true; 
    return status;
}


// mark current page of scan dirty
const Status HeapFileScan::markDirty()
{
    curDirtyFlag = true;
    return OK;
}

const bool HeapFileScan::matchRec(const Record & rec) const
{
    // no filtering requested
    if (!filter) return true;

    // see if offset + length is beyond end of record
    // maybe this should be an error???
    if ((offset + length -1 ) >= rec.length)
	return false;

    float diff = 0;                       // < 0 if attr < fltr
    switch(type) {

    case INTEGER:
        int iattr, ifltr;                 // word-alignment problem possible
        memcpy(&iattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ifltr,
               filter,
               length);
        diff = iattr - ifltr;
        break;

    case FLOAT:
        float fattr, ffltr;               // word-alignment problem possible
        memcpy(&fattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ffltr,
               filter,
               length);
        diff = fattr - ffltr;
        break;

    case STRING:
        diff = strncmp((char *)rec.data + offset,
                       filter,
                       length);
        break;
    }

    switch(op) {
    case LT:  if (diff < 0.0) return true; break;
    case LTE: if (diff <= 0.0) return true; break;
    case EQ:  if (diff == 0.0) return true; break;
    case GTE: if (diff >= 0.0) return true; break;
    case GT:  if (diff > 0.0) return true; break;
    case NE:  if (diff != 0.0) return true; break;
    }

    return false;
}

InsertFileScan::InsertFileScan(const string & name,
                               Status & status) : HeapFile(name, status)
{
  //Do nothing. Heapfile constructor will bread the header page and the first
  // data page of the file into the buffer pool
}

InsertFileScan::~InsertFileScan()
{
    Status status;
    // unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, true);
        curPage = NULL;
        curPageNo = 0;
        if (status != OK) cerr << "error in unpin of data page\n";
    }
}


// Insert a record into the file
/*
Inserts the record described by rec into the file returning the RID of the inserted record in outRid.

TIPS: check if curPage is NULL. If so, make the last page the current page and read it into the buffer. 
Call curPage->insertRecord to insert the record. If successful, remember to DO THE BOOKKEEPING. That is, 
you have to update data fields such as recCnt, hdrDirtyFlag, curDirtyFlag, etc.

If can't insert into the current page, then create a new page, 
initialize it properly, modify the header page content properly, link up 
the new page appropriately, make the current page to be the newly allocated page, 
then try to insert the record. Don't forget bookkeeping that must be done 
after successfully inserting the record.
*/
const Status InsertFileScan::insertRecord(const Record & rec, RID& outRid)
{
    Page*	newPage;
    int		newPageNo;
    Status	status, unpinstatus;
    RID		rid;

    // check for very large records
    if ((unsigned int) rec.length > PAGESIZE-DPFIXED)
    {
        // will never fit on a page, so don't even bother looking
        return INVALIDRECLEN;
    }

    // check if curPage is NULL. If so, make the last page the current page and read it into the buffer. 
    if (curPage == NULL) {
        newPageNo = headerPage->lastPage;
        curPageNo=newPageNo;
        bufMgr->readPage(filePtr, curPageNo, newPage);
        curPage = newPage;
        curDirtyFlag=false;
    }

    // Call curPage->insertRecord
    status = curPage->insertRecord(rec, outRid);

    if (status != OK) {
        //Create a new page
        bufMgr->allocPage(filePtr, newPageNo, newPage);
        //initialize it
        newPage->init(newPageNo);
        
        //modify header page content properly
        headerPage->pageCnt++;
        
        int curLastPage = headerPage->lastPage;
        if (curPageNo == curLastPage) {
            headerPage->lastPage = newPageNo;
            curPage->setNextPage(newPageNo);
            hdrDirtyFlag=true;
            curDirtyFlag=true;
            curPage = newPage;
            status = curPage->insertRecord(rec, outRid);
        } else {
            //link new page properly. This is different if the current page is NOT the last page

            // 1) save current page's next.
            int savedNext = curPage->getNextPage(curPageNo);
            // 2) current page's "next" is now newPage.
            curPage->setNextPage(newPageNo);
            // 3) newPage's next is now curPage's saved next value from step 1.
            newPage->setNextPage(savedNext);

            //bookKeeping?
            //header page's page count increased (already done outside the if else for both cases)
            //headerPage dirty flag true
            //curDirtyFlag is true (next was modified for original page)
            hdrDirtyFlag=true;
            curDirtyFlag=true;

            //make current page newly allocated page
            curPage = newPage;
            //current dirty flag is true; the new page's "next" attribute was altered
            curDirtyFlag=true;

            //try to insert the record again (Return error if doesn't work again??)
            status = newPage->insertRecord(rec, outRid);
        }
    }
  
    //update data fields such as recCnt, hdrDirtyFlag, curDirtyFlag, etc
    headerPage->recCnt++;
    hdrDirtyFlag=true; // What do we set these to?? True or false?
    curDirtyFlag=true;
  
}