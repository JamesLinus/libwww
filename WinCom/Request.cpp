// Request.cpp: implementation of the CRequest class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "WinCom.h"
#include "Request.h"
#include "VersionConflict.h"

// From libwww
#include "WWWLib.h"			      /* Global Library Include file */
#include "WWWApp.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

/////////////////////////////////////////////////////////////////////////////
// Libwww filters

/*
**  Request termination handler
*/
PRIVATE int terminate_handler (HTRequest * request, HTResponse * response,
			       void * param, int status) 
{
    CRequest * req = (CRequest *) HTRequest_context(request);

    req->Cleanup();
	
    delete req;

    return HT_OK;
}

/*
**  412 "Precondition failed" handler
*/
PRIVATE int precondition_handler (HTRequest * request, HTResponse * response,
			          void * param, int status) 
{
    CRequest * req = (CRequest *) HTRequest_context(request);
    CVersionConflict conflict;
    
    req->Cleanup();
    
    if (conflict.DoModal() == IDOK) {
	
	if (conflict.m_versionResolution == 0) {
	    HTAnchor * source = req->Source();
	    HTAnchor * dest = req->Destination();
	    CRequest * new_req = new CRequest(req->m_pDoc);
	    delete req;
	    
	    /* Start a new PUT request without preconditions */
	    new_req->PutDocument(source, dest, FALSE);

	    /* Stop here */
	    return HT_ERROR;

	} else if (conflict.m_versionResolution == 1) {
	    HTAnchor * address = req->Source();
	    CRequest * new_req = new CRequest(req->m_pDoc);
	    delete req;
	    
	    /* Start a new GET request */
	    new_req->GetDocument(address, 2);
	    
	    /* Stop here */
	    return HT_ERROR;

	} else
	    return HT_OK;   /* Just finish the request */
	
    }
    return HT_OK;
}

/*
**  Request HEAD checker filter
*/
PRIVATE int check_handler (HTRequest * request, HTResponse * response,
			   void * param, int status)
{
    CRequest * req = (CRequest *) HTRequest_context(request);
    CVersionConflict conflict;
    
    req->Cleanup();
    
    /*
    ** If head request showed that the document doesn't exist
    ** then just go ahead and PUT it. Otherwise ask for help
    */
    if (status==HT_ERROR || status==HT_INTERRUPTED || status==HT_TIMEOUT) {
	delete req;
	return HT_ERROR;
    } else if (status==HT_NO_ACCESS || status==HT_NO_PROXY_ACCESS) {
	delete req;
	return HT_ERROR;
    } else if (abs(status)/100!=2) {
	HTAnchor * source = req->Source();
	HTAnchor * dest = req->Destination();
	CRequest * new_req = new CRequest(req->m_pDoc);
	delete req;
	
	/* Start a new PUT request without preconditions */
	new_req->PutDocument(source, dest, FALSE);
	
    } else if (conflict.DoModal() == IDOK) {
	
	if (conflict.m_versionResolution == 0) {
	    HTAnchor * source = req->Source();
	    HTAnchor * dest = req->Destination();
	    CRequest * new_req = new CRequest(req->m_pDoc);
	    delete req;
	    
	    /* Start a new PUT request without preconditions */
	    new_req->PutDocument(source, dest, FALSE);
	    
	} else if (conflict.m_versionResolution == 1) {
	    HTAnchor * address = req->Source();
	    CRequest * new_req = new CRequest(req->m_pDoc);
	    delete req;
	    
	    /* Start a new GET request  */
	    new_req->GetDocument(address, 2);
        }
    }
    return HT_ERROR;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CRequest, CObject)

CRequest::CRequest(CWinComDoc * doc)
{
    ASSERT(doc != NULL);
    m_pDoc = doc;

    m_pHTRequest = NULL;
    m_pSource = NULL;
    m_pDestination = NULL;
    m_file = NULL;

    // @@@ADD TO DOC LIST@@@
}

CRequest::~CRequest()
{
    // @@@REMOVE FROM DOC LIST@@@
}

/////////////////////////////////////////////////////////////////////////////
// CWinComDoc commands

BOOL CRequest::SetCacheValidation(int mode)
{
    ASSERT(m_pHTRequest != NULL);
    HTReload reload = HT_CACHE_OK;
    switch (mode) {
    case 0:
        reload = HT_CACHE_OK;
        break;
    case 1:
        reload = HT_CACHE_VALIDATE;
        break;
    case 2:
        reload = HT_CACHE_END_VALIDATE;
        break;
    case 3:
        reload = HT_CACHE_FLUSH;
        break;
    default:
        reload = HT_CACHE_OK;
    }
 
    // Set the cache validation mode for this request
    HTRequest_setReloadMode(m_pHTRequest, reload);

    return TRUE;
}

int CRequest::GetDocument (HTAnchor * address, int cacheValidation)
{
    ASSERT(address != NULL); 

    m_pSource = address;
    
    CWinComApp * pApp = (CWinComApp *) AfxGetApp();
    ASSERT(pApp != NULL); 
    
    // Find the file name where we should save the document
    CFileDialog fd(FALSE);
    fd.m_ofn.lpstrInitialDir = pApp->GetIniCWD();

    if (fd.DoModal() == IDOK) {
	
	m_saveAs = fd.GetPathName();
	
	// Set the initial dir for next time
	CString path = m_saveAs;    
	int idx = path.ReverseFind('\\');
	if (idx != -1) path.GetBufferSetLength(idx+1);
	pApp->SetIniCWD(path);
	
        /* Create a new libwww request object */
        m_pHTRequest = HTRequest_new();
	
	/* Set the context so that we can find it again */
	HTRequest_setContext(m_pHTRequest, this);

        /* Terminating filters */
	InitializeFilters();

        /* Should we do cache validation? */
        SetCacheValidation(cacheValidation);
        
        /* Progress bar */
        m_pProgress = new CProgress(this);
	m_pProgress->Create(IDD_PROGRESS);
	m_pProgress->ShowWindow(SW_SHOW);

	/* Start the GET */
	char * addr = HTAnchor_address(address);
        if (HTLoadToFile (addr, m_pHTRequest, m_saveAs) == NO) {
            HT_FREE(addr);
	    return -1;
        }
        HT_FREE(addr);

        return 0;
    }
    return -1;
}

int CRequest::HeadDocument (HTAnchor * address, int cacheValidation)
{
    ASSERT(address != NULL); 

    m_pSource = address;
    
    /* Create a new libwww request object */
    m_pHTRequest = HTRequest_new();
        
    /* Set the context so that we can find it again */
    HTRequest_setContext(m_pHTRequest, this);
    
    /* Should we do cache validation? */
    SetCacheValidation(cacheValidation);

    /* Terminating filters */
    InitializeFilters();

    /* Progress bar */
    m_pProgress = new CProgress(this);
    m_pProgress->Create(IDD_PROGRESS);
    m_pProgress->ShowWindow(SW_SHOW);
    
    /* Start the HEAD */
    if (HTHeadAnchor(address, m_pHTRequest) == NO) {
	return -1;
    }
    return 0;
}

int CRequest::DeleteDocument (HTAnchor * address, BOOL UsePreconditions)
{
    ASSERT(address != NULL); 

    m_pSource = address;
    
    /* Create a new libwww request object */
    m_pHTRequest = HTRequest_new();
        
    /* Set the context so that we can find it again */
    HTRequest_setContext(m_pHTRequest, this);
    
    /* Should we use preconditions? */
    if (UsePreconditions)
	HTRequest_setPreconditions(m_pHTRequest, YES);

    /* Terminating filters */
    InitializeFilters();

    /* Progress bar */
    m_pProgress = new CProgress(this);
    m_pProgress->Create(IDD_PROGRESS);
    m_pProgress->ShowWindow(SW_SHOW);
    
    /* Start the HEAD */
    if (HTDeleteAnchor(address, m_pHTRequest) == NO) {
	return -1;
    }
    return 0;
}

int CRequest::PutDocument (HTAnchor * source,
			   HTAnchor * destination,
			   BOOL UsePreconditions)
{
    ASSERT(source != NULL); 
    ASSERT(destination != NULL); 

    m_pSource = source;
    m_pDestination = destination;
    
    /* Create a new libwww request object */
    m_pHTRequest = HTRequest_new();
    
    /* Set the context so that we can find it again */
    HTRequest_setContext(m_pHTRequest, this);
    
    /* Should we use preconditions? */
    if (UsePreconditions)
	HTRequest_setPreconditions(m_pHTRequest, YES);
	
    /* Terminating filters */
    InitializeFilters();

    /* Progress bar */
    m_pProgress = new CProgress(this);
    m_pProgress->Create(IDD_PROGRESS);
    m_pProgress->ShowWindow(SW_SHOW);

    /* Start the PUT */    
    if (HTPutDocumentAnchor(HTAnchor_parent(source), destination, m_pHTRequest) != YES) {
	return -1;
    }
    return 0;
}

int CRequest::PutDocumentWithPrecheck (HTAnchor * source,
				       HTAnchor * destination,
				       BOOL UsePreconditions)
{
    ASSERT(source != NULL); 
    ASSERT(destination != NULL); 

    m_pSource = source;
    m_pDestination = destination;
    
    /* Create a new libwww request object */
    m_pHTRequest = HTRequest_new();
    
    /* Only use authentication and check AFTER filters */
    HTRequest_addAfter(m_pHTRequest, HTAuthFilter,	"http://*", NULL, HT_NO_ACCESS, HT_FILTER_MIDDLE, YES);
    HTRequest_addAfter(m_pHTRequest, HTAuthFilter, 	"http://*", NULL, HT_REAUTH,    HT_FILTER_MIDDLE, YES);
    HTRequest_addAfter(m_pHTRequest, HTUseProxyFilter,	"http://*", NULL, HT_USE_PROXY, HT_FILTER_MIDDLE, YES);
    HTRequest_addAfter(m_pHTRequest, check_handler,	NULL,	    NULL, HT_ALL,       HT_FILTER_MIDDLE, YES);
    
    /* Set the context so that we can find it again */
    HTRequest_setContext(m_pHTRequest, this);
    
    /* Set cache validation */
    SetCacheValidation(2);

    /* Progress bar */
    m_pProgress = new CProgress(this);
    m_pProgress->Create(IDD_PROGRESS);
    m_pProgress->ShowWindow(SW_SHOW);

    /* Start the HEAD */
    if (HTHeadAnchor(destination, m_pHTRequest) == NO) {
	return -1;
    }
    return 0;
}

int CRequest::Cancel()
{
    if (m_pHTRequest) HTRequest_kill(m_pHTRequest);
    if (m_pProgress) m_pProgress = NULL;
    return 0;
}

BOOL CRequest::Cleanup()
{
    if (m_file) {
	fclose(m_file);
	m_file = NULL;
    }
    if (m_pProgress) {
	m_pProgress->DestroyWindow();
	m_pProgress = NULL;
    }
    return TRUE;
}

HTAnchor * CRequest::Source()
{
    return m_pSource;
}

HTAnchor * CRequest::Destination()
{
    return m_pDestination;
}

BOOL CRequest::InitializeFilters()
{
    static BOOL initialized = NO;
    if (initialized == NO) {

	/* Add our own after filter to handle 412 precondition failed */
        HTNet_addAfter(precondition_handler, NULL, NULL, HT_PRECONDITION_FAILED, HT_FILTER_MIDDLE);

        /* Add our own after filter to clecn up */
        HTNet_addAfter(terminate_handler, NULL, NULL, HT_ALL, HT_FILTER_LAST);

	initialized = YES;

	return TRUE;
    }
    return FALSE;
}

CProgressCtrl * CRequest::GetProgressBar()
{
    return &(m_pProgress->m_progress);
}

