#include "kpdf_part.h"

#include <math.h>

#include <qwidget.h>
#include <qlistbox.h>
#include <qfile.h>
#include <qtimer.h>

#include <kaction.h>
#include <kdebug.h>
#include <kinstance.h>
#include <kprinter.h>
#include <kstdaction.h>
#include <kparts/genericfactory.h>

#include <kdebug.h>

#include "GString.h"

#include "GlobalParams.h"
#include "PDFDoc.h"
#include "XOutputDev.h"

// #include "kpdf_canvas.h"
// #include "kpdf_pagewidget.h"

#include "part.h"

typedef KParts::GenericFactory<KPDF::Part> KPDFPartFactory;
K_EXPORT_COMPONENT_FACTORY(libkpdfpart, KPDFPartFactory);

using namespace KPDF;

Part::Part(QWidget *parentWidget, const char *widgetName,
           QObject *parent, const char *name,
           const QStringList & /*args*/ )
  : KParts::ReadOnlyPart(parent, name),
    m_doc(0),
    m_currentPage(0),
    m_zoomMode(FixedFactor),
    m_zoomFactor(1.0)
{
  new BrowserExtension(this);

  globalParams = new GlobalParams("");

  // we need an instance
  setInstance(KPDFPartFactory::instance());

	pdfpartview = new PDFPartView(parentWidget, widgetName);

	connect(pdfpartview->pagesListBox, SIGNAL( clicked ( QListBoxItem * ) ),
			this, SLOT( pageClicked ( QListBoxItem * ) ));
	
  m_outputDev = pdfpartview->outputdev;
  setWidget(pdfpartview);

  // create our actions
  KStdAction::find    (this, SLOT(find()), 
                       actionCollection(), "find");
  KStdAction::findNext(this, SLOT(findNext()), 
                       actionCollection(), "find_next");

  m_fitToWidth = new KToggleAction(i18n("Fit to Page &Width"), 0,
                       this, SLOT(slotFitToWidthToggled()),
                       actionCollection(), "fit_to_width");
  KStdAction::zoomIn  (this, SLOT(zoomIn()),
                       actionCollection(), "zoom_in");
  KStdAction::zoomOut (this, SLOT(zoomOut()),
                       actionCollection(), "zoom_out");

  KStdAction::back    (this, SLOT(back()),
                       actionCollection(), "back");
  KStdAction::forward (this, SLOT(forward()),
                       actionCollection(), "forward");
  KStdAction::prior   (this, SLOT(displayPreviousPage()), 
                       actionCollection(), "previous_page");
  KStdAction::next    (this, SLOT(displayNextPage()),
                       actionCollection(), "next_page" );

  // set our XML-UI resource file
  setXMLFile("kpdf_part.rc");
}

Part::~Part()
{
}

  KAboutData*
Part::createAboutData()
{
  // the non-i18n name here must be the same as the directory in
  // which the part's rc file is installed ('partrcdir' in the
  // Makefile)
  KAboutData* aboutData = new KAboutData("kpdfpart", I18N_NOOP("KPDF::Part"), 
                                         "0.1");
  aboutData->addAuthor("Wilco Greven", 0, "greven@kde.org");
  return aboutData;
}

  bool
Part::closeURL()
{
  delete m_doc;
  m_doc = 0;
  
  return KParts::ReadOnlyPart::closeURL();
}

  bool
Part::openFile()
{
  // m_file is always local so we can use QFile on it
  QFile file(m_file);
  if (file.open(IO_ReadOnly) == false)
    return false;

  GString* filename = new GString(m_file);
  m_doc = new PDFDoc(filename, 0, 0);

  if (!m_doc->isOk())
    return false;

  // just for fun, set the status bar
  // emit setStatusBarText( QString::number( m_doc->getNumPages() ) );

	// fill the listbox with entries for every page
	pdfpartview->pagesListBox->setUpdatesEnabled(false);
	pdfpartview->pagesListBox->clear();
	for (int i = 1; i <= m_doc->getNumPages(); i++)
	{
		pdfpartview->pagesListBox->insertItem(QString("%1").arg(i));
	}
	pdfpartview->pagesListBox->setUpdatesEnabled(true);
	pdfpartview->pagesListBox->update();
	
  displayPage(1);

  return true;
}

  void 
Part::displayPage(int pageNumber, float /*zoomFactor*/)
{
	kdDebug() << "display page" << endl;
	kdDebug() << "page : " << pageNumber << endl;
	kdDebug() << "zoom factor : " << m_zoomFactor << endl;

	Page * p = m_doc->getCatalog()->getPage(pageNumber);
	
	kdDebug() << "metadata stream : " << endl;
	char * md = new char[4096];
	
	if (p->getMetadata())
	{
		while(p->getMetadata()->getLine(md, 4096) != NULL)
		{
			kdDebug() << md << endl;
		}
	}

	kdDebug() << "------------" << endl;
		
  if (pageNumber <= 0 || pageNumber > m_doc->getNumPages())
    return;

  const double pageWidth  = m_doc->getPageWidth (pageNumber) * m_zoomFactor;
  const double pageHeight = m_doc->getPageHeight(pageNumber) * m_zoomFactor;

  // Pixels per point when the zoomFactor is 1.
  const float basePpp  = QPaintDevice::x11AppDpiX() / 72.0;

  switch (m_zoomMode)
  {
  case FitWidth:
  {
    const double pageAR = pageWidth/pageHeight; // Aspect ratio

    const int canvasWidth    = m_outputDev->contentsRect().width();
    const int canvasHeight   = m_outputDev->contentsRect().height();
    const int scrollBarWidth = m_outputDev->verticalScrollBar()->width();

    // Calculate the height so that the page fits the viewport width 
    // assuming that we need a vertical scrollbar.
    float height = float(canvasWidth - scrollBarWidth) / pageAR;

    // If the vertical scrollbar wasn't needed after all, calculate the page
    // size so that the page fits the viewport width without the scrollbar.
    if (ceil(height) <= canvasHeight)
    {
      height = float(canvasWidth) / pageAR;

      // Handle the rare case that enlarging the page resulted in the need of 
      // a vertical scrollbar. We can fit the page to the viewport height in
      // this case.
      if (ceil(height) > canvasHeight)
        height = float(canvasHeight) * pageAR;
    }

    m_zoomFactor = (height / pageHeight) / basePpp;
    break;
  }
  case FixedFactor:
  default:
    break;
  }


  const float ppp = basePpp * m_zoomFactor; // pixels per point

  m_doc->displayPage(m_outputDev, pageNumber, int(m_zoomFactor * ppp * 72.0), 0, true);

  m_outputDev->show();

  m_currentPage = pageNumber;
}

  void
Part::displayDestination(LinkDest* dest)
{
  int pageNumber;
  // int dx, dy;

  if (dest->isPageRef()) 
  {
    Ref pageRef = dest->getPageRef();
    pageNumber = m_doc->findPage(pageRef.num, pageRef.gen);
  }
  else 
  {
    pageNumber = dest->getPageNum();
  }

  if (pageNumber <= 0 || pageNumber > m_doc->getNumPages()) 
  {
    pageNumber = 1;
  }

  displayPage(pageNumber);
  return;
/*
  if (fullScreen) {
    return;
  }
  switch (dest->getKind()) {
  case destXYZ:
    out->cvtUserToDev(dest->getLeft(), dest->getTop(), &dx, &dy);
    if (dest->getChangeLeft() || dest->getChangeTop()) {
      if (dest->getChangeLeft()) {
	hScrollbar->setPos(dx, canvas->getWidth());
      }
      if (dest->getChangeTop()) {
	vScrollbar->setPos(dy, canvas->getHeight());
      }
      canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    }
    //~ what is the zoom parameter?
    break;
  case destFit:
  case destFitB:
    //~ do fit
    hScrollbar->setPos(0, canvas->getWidth());
    vScrollbar->setPos(0, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    break;
  case destFitH:
  case destFitBH:
    //~ do fit
    out->cvtUserToDev(0, dest->getTop(), &dx, &dy);
    hScrollbar->setPos(0, canvas->getWidth());
    vScrollbar->setPos(dy, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    break;
  case destFitV:
  case destFitBV:
    //~ do fit
    out->cvtUserToDev(dest->getLeft(), 0, &dx, &dy);
    hScrollbar->setPos(dx, canvas->getWidth());
    vScrollbar->setPos(0, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    break;
  case destFitR:
    //~ do fit
    out->cvtUserToDev(dest->getLeft(), dest->getTop(), &dx, &dy);
    hScrollbar->setPos(dx, canvas->getWidth());
    vScrollbar->setPos(dy, canvas->getHeight());
    canvas->scroll(hScrollbar->getPos(), vScrollbar->getPos());
    break;
  }
*/
}

  void
Part::print()
{
  if (m_doc == 0)
    return;

  KPrinter printer;

  printer.setPageSelection(KPrinter::ApplicationSide);
  printer.setCurrentPage(m_currentPage);
  printer.setMinMax(1, m_doc->getNumPages());

  if (printer.setup(widget())) 
  {
    /*
    KTempFile tf(QString::null, ".ps");
    if (tf.status() == 0) 
    {
      savePages(tf.name(), printer.pageList());
      printer.printFiles(QStringList( tf.name() ), true);
    }
    else 
    {
       // TODO: Proper error handling
      ;
    }
    */
  }
}

  void 
Part::displayNextPage()
{
  displayPage(m_currentPage + 1);
}

  void 
Part::displayPreviousPage()
{
  displayPage(m_currentPage - 1);
}

/*
  void 
Part::setFixedZoomFactor(float zoomFactor)
{

}
*/

  void 
Part::executeAction(LinkAction* action)
{
  if (action == 0)
    return;

  LinkActionKind kind = action->getKind();

  switch (kind) 
  {
  case actionGoTo:
  case actionGoToR:
  {
    LinkDest* dest = 0;
    GString* namedDest = 0; 

    if (kind == actionGoTo) 
    {
      if ((dest = ((LinkGoTo*)action)->getDest()))
        dest = dest->copy();
      else if ((namedDest = ((LinkGoTo*)action)->getNamedDest()))
        namedDest = namedDest->copy();
    }
    /*
    else
    {
      if ((dest = ((LinkGoToR*)action)->getDest()))
        dest = dest->copy();
      else if ((namedDest = ((LinkGoToR*)action)->getNamedDest()))
        namedDest = namedDest->copy();
      s = ((LinkGoToR*)action)->getFileName()->getCString();
      //~ translate path name for VMS (deal with '/')
      if (!loadFile(fileName)) 
      {
        delete dest;
        delete namedDest;
        return;
      }
    }
    */

    if (namedDest != 0)
    {
      dest = m_doc->findDest(namedDest);
      delete namedDest;
    }
    if (dest != 0) 
    {
      displayDestination(dest);
      delete dest;
    }
    else
    {
      if (kind == actionGoToR)
        displayPage(1);
    }
    break;
  }
  default:
      break;
  }
}

  void
Part::slotFitToWidthToggled()
{
  m_zoomMode = m_fitToWidth->isChecked() ? FitWidth : FixedFactor;
  displayPage(m_currentPage);
}

// for optimization
bool redrawing = false;

void
Part::update()
{
	if (m_outputDev && ! redrawing) 
	{
		redrawing = true;
		QTimer::singleShot(200, this, SLOT( redrawPage() ));
	}
};

void 
Part::redrawPage()
{ 
	redrawing = false;
	displayPage(m_currentPage); 
}

void 
Part::pageClicked ( QListBoxItem * qbi )
{
	m_currentPage = pdfpartview->pagesListBox->index(qbi);
	update();
}
	
BrowserExtension::BrowserExtension(Part* parent)
  : KParts::BrowserExtension( parent, "KPDF::BrowserExtension" )
{
  emit enableAction("print", true);
  setURLDropHandlingEnabled(true);
}

  void 
BrowserExtension::print()
{
  static_cast<Part*>(parent())->print();
}

#include "kpdf_part.moc"

// vim:ts=2:sw=2:tw=78:et
