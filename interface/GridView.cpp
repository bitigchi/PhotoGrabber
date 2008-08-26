/*****************************************************************
* Copyright (c) 2004-2008,	Ramshankar, Jan-Rixt Van Hoye		 *
* All rights reserved.											 *
* Distributed under the terms of the MIT License.                *
 *****************************************************************/

#include <app/Messenger.h>
#include <app/Looper.h>
#include <app/Handler.h>
#include <interface/Bitmap.h>
#include <interface/ScrollView.h>
#include <interface/Window.h>
#include <support/Debug.h>
#include <support/List.h>
#include <support/String.h>

#include "GridView.h"
#include "debug.h"
#include "core_global.h"

FILE *lfgridv;

#define kDRAG_SLOP		4

float GridView::fItemWidth	= 160;	// max width of a bitmap
float GridView::fItemHeight	= 120;	// max height of a bitmap
float GridView::fItemMargin = 20;	// margin
//
//	GridView :: Constructor
GridView::GridView (BRect rect, const char* name, uint32 resize,uint32 flags)
: BView (rect, name, resize, flags | B_FRAME_EVENTS)
	,fCachedColumnCount (-1)
	,fSelectedItemIndex (-1)
	,fSelectionRadius (4)
	,fScrollView (NULL)
	,fTargetMessenger (NULL)
	,fKeyTargetLooper (NULL)
	,fKeyTargetHandler (NULL)
{
	rgb_color color_background = ui_color(B_MENU_BACKGROUND_COLOR);
	SetViewColor(color_background);
	fItemList = new BList();
}

//
//	GridView :: Destructor
GridView::~GridView()
{
	DeleteAllItems ();
	delete fItemList;
	fItemList = NULL;
	
	if (fTargetMessenger)
	{
		delete fTargetMessenger;
		fTargetMessenger = NULL;
	}
}

//
//	GridView :: Set Target
void GridView::SetTarget (BMessenger& messenger)
{
	fTargetMessenger = new BMessenger (messenger);
}

//
//	GridView :: Add Item
void GridView::AddItem (BeCam_Item* item)
{
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Add Item\n");
		fclose(lfgridv);
	#endif
	AddItemFast (item);
	UpdateScrollView();
}

//
//	GridView :: Add Item Fast
inline void
GridView::AddItemFast (BeCam_Item* item)
{
	fItemList->AddItem (reinterpret_cast<void*>(item));
}
//
//	GridView :: Remove Item
void	GridView::RemoveItem(BeCam_Item* item)
{
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Remove Item\n");
		fclose(lfgridv);
	#endif
	RemoveItemFast (item);
	UpdateScrollView();
}
//
//	GridView :: Remove Item Fast
inline void GridView::RemoveItemFast (BeCam_Item* item)
{
	fItemList->RemoveItem (reinterpret_cast<void*>(item));
}
//
//	GridView :: AddList
void GridView::AddList (BList& list)
{
	int32 count = list.CountItems();
	for (int32 i = 0; i < count; i++)
		AddItemFast ((BeCam_Item*)list.ItemAtFast(i));
	
	fCachedColumnCount = -1;
	UpdateScrollView();
}

//
// GridView :: Draw
void GridView::Draw (BRect rect)
{
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Draw\n");
		fclose(lfgridv);
	#endif
	DrawContent (rect);
	return BView::Draw (rect);
}

//
// GridView :: DrawContent
void GridView::DrawContent (BRect /*_unused*/)
{

	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Draw Content\n");
		fclose(lfgridv);
	#endif
	SetHighColor (ViewColor());
	if (fItemList == NULL || fItemList->CountItems() == 0)
	{
		FillRect (Bounds());
		return;
	}
	
	BRect bounds = Bounds();
	int32 columnCount = CountColumns();

	
	BRect toEraseRect;
	if (fCachedColumnCount > columnCount)
	{
		toEraseRect.Set (columnCount * (ItemWidth() + ItemMargin()), 0, bounds.right, bounds.bottom);
		FillRect (toEraseRect);
	}
	else
	{
		if (columnCount < 1)
			columnCount = 1;
		int32 rowCount = (int32)ceil ((double)(fItemList->CountItems()) / (double)columnCount);
			
		int32 remainingInRow = fItemList->CountItems() % columnCount;
		toEraseRect.Set (0,rowCount * (ItemHeight() + ItemMargin()),bounds.right, bounds.bottom);
		FillRect (toEraseRect); 
		
		// Erase partial row content (those items have been moved left thus needs erasing)
		if (remainingInRow > 0)
		{
			float top = ItemHeight();
			toEraseRect.Set (remainingInRow * (ItemWidth() + ItemMargin()), (rowCount - 2) * top - 1, bounds.right, rowCount * top);
			FillRect (toEraseRect);
		}
	}

	fCachedColumnCount = columnCount;
	int32 x = 0;
	int32 y = 0;
	for (int32 i = 0; i < fItemList->CountItems(); i++)
	{
		BeCam_Item *item = (BeCam_Item*)(fItemList->ItemAt (i));
		if (item == NULL)
			break;
		
		BRect itemRect;
		
		itemRect.left = x * (ItemWidth() + ItemMargin()) ;
		itemRect.top = y * (ItemHeight() + ItemMargin()); 
		
		itemRect.right = itemRect.left + ItemWidth() + ItemMargin();
		itemRect.bottom = itemRect.top + ItemHeight() + ItemMargin();
		// Draw the item
		item->DrawItem(this, itemRect, false);
		//
		x++;
		if (x >= columnCount)
		{
			x = 0;
			y++;
		}
	}
}

//
// GridView :: FrameResized
void GridView::FrameResized (float newWidth, float newHeight)
{
	if (CountColumns() != fCachedColumnCount)
		Draw (BRect (0, 0, 0, 0));
	
	UpdateScrollView();
	return BView::FrameResized (newWidth, newHeight);
}

//
// GridView :: ItemRect
BRect GridView::ItemRect (int32 index)
{
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Item Rect of index %d\n",index);
		fclose(lfgridv);
	#endif
	
	int32 columnCount = CountColumns();
	int32 x = (index % columnCount);
	int32 y = index / columnCount;

	BRect itemRect;
	
	itemRect.left = x * (ItemWidth () + ItemMargin());
	itemRect.top = y * (ItemHeight () + ItemMargin());
	itemRect.right = itemRect.left + ItemWidth() + ItemMargin();
	itemRect.bottom = itemRect.top + ItemHeight() + ItemMargin();
	return itemRect;
}

//
//	GridView :: CountColumns
int32 GridView::CountColumns () const
{
	return (int32)(Bounds().Width()/ (ItemWidth() + ItemMargin()));
}


//
//	GridView :: CountRows
int32 GridView::CountRows () const
{
	return (int32)ceil ((double)(fItemList->CountItems()) / (double)CountColumns());
}

//
//	GridView :: KeyDown
void GridView::KeyDown (const char* bytes, int32 numBytes)
{
	if (!Window() || Window()->IsActive() == false)
		return;

	bool keyHandled = HandleKeyMovement (bytes, numBytes);
	if (fKeyTargetLooper)
	{
		BMessage *curMessage = Window()->CurrentMessage();
		curMessage->AddBool ("key_handled", keyHandled);
		fKeyTargetLooper->PostMessage (curMessage);
	}
	
	return BView::KeyDown (bytes, numBytes);
}

//
//	GridView :: MouseDown
void GridView::MouseDown (BPoint point)
{
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Mouse Down\n");
		fclose(lfgridv);
	#endif
	if (!Window() || Window()->IsActive() == false)
		return;
	
	BMessage* msg = Window()->CurrentMessage();
	int32 clicks = msg->FindInt32 ("clicks");
	int32 button = msg->FindInt32 ("buttons");
	static BPoint previousPoint = point;
	static int32 lastButton = -1;
	static int32 clickCount = 1;

	if ((button == lastButton) && (clicks > 1))
		clickCount++;
	else
		clickCount = 1;

	lastButton = button;
	
	// Check if the user clicked once and intends to drag
	int index = IndexOf(point);
	if ((clickCount == 1) || (index != CurrentSelection()))
    {
       	//int32 modifs = modifiers();
		// select this item
      	//if ((modifs & B_OPTION_KEY) || (modifs & B_SHIFT_KEY))
      	Select(index);
        //else
        //  Select(index);
        // create a structure of
        // useful data
        list_tracking_data *data = new list_tracking_data();
        data->start = point;
        data->view = this;
        // spawn a thread that watches
        // the mouse to see if a drag
        // should occur.  this will free
        // up the window for more
        // important tasks
        resume_thread(spawn_thread((status_t(*)(void*))TrackItem,"list_tracking",B_DISPLAY_PRIORITY,data));
        return;
    }
	previousPoint = point;
	return BView::MouseDown (point);
}

//
//	GridView :: MouseUp
void GridView::MouseUp (BPoint point)
{
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Mouse Up\n");
		fclose(lfgridv);
	#endif
	if (!Window() || Window()->IsActive() == false)
		return;

	int32 itemIndex = IndexOf(point);
	
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Item index = %d\n",itemIndex);
		fclose(lfgridv);
	#endif
	
	BRect itemRect = ItemRect (itemIndex);
	BeCam_Item *item = reinterpret_cast<BeCam_Item*>(fItemList->ItemAt(itemIndex));

	if (itemRect.Contains (point) && item)
	{
		#ifdef DEBUG
			lfgridv = fopen(INTF_LOGFILE,"a");	
			fprintf(lfgridv,"GRIDVIEW - Select Item with index %d\n",itemIndex);
			fclose(lfgridv);
		#endif
		Select (itemIndex);
		ScrollToSelection ();
	}
	else
		DeselectAll ();

	if (IsFocus() == false)
		MakeFocus (true);

	return BView::MouseUp (point);
}


//
//	GridView :: Count Items
int32 GridView::CountItems() const
{
	return fItemList->CountItems();
}

//
//	GridView :: Delete All Items
void GridView::DeleteAllItems ()
{
	for (int32 i = 0; i < fItemList->CountItems(); i++)
	{
		BeCam_Item *item = reinterpret_cast<BeCam_Item*>(fItemList->RemoveItem (0L));
		delete item;
		item = NULL;
	}

	fItemList->MakeEmpty();
	Draw (Bounds());
}

//
//	GridView :: Targeted By ScrollView
void GridView::TargetedByScrollView (BScrollView* scrollView)
{
	fScrollView = scrollView;
	UpdateScrollView();
}

//
// GridView :: AttachedToWindow
void GridView::AttachedToWindow()
{
	UpdateScrollView ();
	return BView::AttachedToWindow();
}

//
//	GridView :: Updated ScrollView
void GridView::UpdateScrollView ()
{	
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Update ScrollView\n");
		fclose(lfgridv);
	#endif
	if (fScrollView)
	{
		BScrollBar *vertbar = fScrollView->ScrollBar (B_VERTICAL);
		if (vertbar)
		{
			float maxV = CountRows() * (ItemHeight() + ItemMargin());

			if (maxV - Bounds().Height() > 0)
				vertbar->SetRange (0, maxV - Bounds().Height());
			else
				vertbar->SetRange (0, 0);
			
			vertbar->SetSteps ((ItemHeight() + ItemMargin()) / 2, (ItemHeight() + ItemMargin()));
		}
	}
	Invalidate();
}

//
//	GridView :: Handle KeyMovement
bool GridView::HandleKeyMovement (const char* bytes, int32 /* _unused */)
{
	bool keyHandled = false;
	switch (bytes[0])
	{
		case B_RIGHT_ARROW:
		{
			keyHandled = true;

			// The if check simply prevents going to the next row, remove if needed
			//if ((fSelectedItemIndex + 1) % CountColumns() != 0 || fSelectedItemIndex == 0)
			Select (fSelectedItemIndex < CountItems() - 1 ? fSelectedItemIndex + 1 : CountItems() - 1);

			break;
		}

		case B_LEFT_ARROW:
		{
			keyHandled = true;

			// The if check simply prevents going to the previous row, remove if needed
			//if ((fSelectedItemIndex + 1) % CountColumns() != 1 || fSelectedItemIndex == CountItems() - 1)
				Select (fSelectedItemIndex > 0 ? fSelectedItemIndex - 1 : 0);

			break;
		}
		
		case B_DOWN_ARROW:
		{
			keyHandled = true;

			if (fSelectedItemIndex < 0)
			{
				Select (0);
				break;
			}
			
			int32 index = CountColumns() + fSelectedItemIndex;
			Select (index <= CountItems() - 1 ? index : fSelectedItemIndex);
			break;
		}

		case B_UP_ARROW:
		{
			keyHandled = true;

			if (fSelectedItemIndex - CountColumns() < 0)
			{
				Select (fSelectedItemIndex);
				break;
			}
			
			int32 index = fSelectedItemIndex - CountColumns();
			Select (index <= CountItems() - 1 ? index : fSelectedItemIndex);
			break;
		}

		case B_HOME:
		{
			keyHandled = true;

			Select (0);
			break;
		}
		
		case B_END:
		{
			keyHandled = true;

			Select (CountItems() - 1);
			break;
		}
		
		case B_PAGE_DOWN:
		{
			keyHandled = true;

			int8 rowsPerPage = (int8)(Bounds().Height() / ItemHeight());
			int32 index = fSelectedItemIndex + rowsPerPage * CountColumns();
			int32 finalRowIndex = (CountRows() - 1) * CountColumns();
			int32 whichColumn = fSelectedItemIndex % CountColumns();

			Select (index <= CountItems() - 1 ? index : MIN (finalRowIndex + whichColumn, CountItems() - 1));
			break;
		}
		
		case B_PAGE_UP:
		{
			keyHandled = true;

			int8 rowsPerPage = (int8)(Bounds().Height() / ItemHeight());
			int32 index = fSelectedItemIndex - rowsPerPage * CountColumns();
			int32 whichColumn = fSelectedItemIndex % CountColumns();

			Select (index >= 0 ? index : whichColumn);
			break;
		}
	}
	
	ScrollToSelection ();
	return keyHandled;
}

//
//	GridView :: ScrollToSelection
void GridView::ScrollToSelection ()
{
	if (!fSelectedItem)
		return;
	
	float currentPosition = 0;
	if (fScrollView)
	{
		BScrollBar *vertbar = fScrollView->ScrollBar (B_VERTICAL);
		if (vertbar)
			currentPosition = vertbar->Value();
	}

	BRect rect = ItemRect (fSelectedItemIndex);
	if (rect.bottom >= currentPosition + Bounds().Height())		// down
	{
		ScrollTo (0, rect.top + 2 - (Bounds().Height() - ItemHeight() -ItemMargin()));
		printf ("%f\n", rect.top + 2 - (Bounds().Height() - ItemHeight() - ItemMargin()));
	}	
	else if (rect.top <= currentPosition)				// up
		ScrollTo (0, rect.top - 4);
}

//
//	GridView :: ItemHeight
float GridView::ItemHeight () const
{
	return fItemHeight;
}

//
//	GridView :: ItemWidth
float GridView::ItemWidth () const
{
	return fItemWidth;
}

//
//	GridView :: ItemMargin
float GridView::ItemMargin () const
{
	return fItemMargin;
}

//
// GridView :: Selected Item
BeCam_Item* GridView::SelectedItem () const
{
	return fSelectedItem;
}

//
//	GridView :: ItemAt
BeCam_Item* GridView::ItemAt (int32 index) const
{
	return (BeCam_Item*)fItemList->ItemAt (index);
}

//
//	GridView :: SetSelectionCurveRadius
void GridView::SetSelectionCurveRadius (uint8 radius)
{
	fSelectionRadius = (int8)(radius - 1);		// internally subtract one because of how we
										// actually handle it in Draw()
	// So basically if they specify radius of zero (0) meaning no curve, ONLY
	// if we subtract one do we get that effect
	
	fSelectionRadius = MIN (fSelectionRadius, 6);
}

//
//	GridView :: SelectionCurveRadius
uint8 GridView::SelectionCurveRadius () const
{
	return (uint8)(fSelectionRadius + 1);
}

//
//	GridView :: SendKeyStrokesTo
void GridView::SendKeyStrokesTo (BLooper* looper, BHandler* handler)
{
	fKeyTargetLooper = looper;
	fKeyTargetHandler = handler;
}
//
//	GridView :: Select
void GridView::Select (int32 index)
{
	#ifdef DEBUG
		lfgridv = fopen(INTF_LOGFILE,"a");	
		fprintf(lfgridv,"GRIDVIEW - Select Item\n");
		fclose(lfgridv);
	#endif
	int32 modifs = modifiers();
	
	if ((modifs & B_OPTION_KEY))
	{
		// 1) Click + Option Key
		
	}
	else if((modifs & B_SHIFT_KEY))
	{
		// 2) Click + Shift Key
	}
	else
	{
		// 3) Click
		
		if (index == fSelectedItemIndex)
			return;
	
		if (fSelectedItem != NULL)
			Deselect (fSelectedItemIndex);
	
		if (index < 0 || index > fItemList->CountItems() - 1)
		{
			fSelectedItem = NULL;
			fSelectedItemIndex = -1;
			return;
		}
		
		BeCam_Item *item = reinterpret_cast<BeCam_Item*>(fItemList->ItemAt(index));
		item->Select ();

		fSelectedItem = item;
		fSelectedItem->DrawItem (this, ItemRect (index), false);
		fSelectedItemIndex = index;
	}
}
//	GridView :: Select From To
void GridView :: Select (int32 fromIndex, int32 toIndex)
{
	BeCam_Item *item;
	for(int32 i = fromIndex;i <= toIndex;i++)
	{
		item = reinterpret_cast<BeCam_Item*>(fItemList->ItemAt(i));
		if(item != NULL)
			item->Select();
	}
}
//
//	GridView:: Select All Items
void 	GridView::SelectAll ()
{
	for (int32 i = 0; i < fItemList->CountItems(); i++)
	{
		Select(i);
	}
}

//
//	GridView :: Deselect
void GridView::Deselect(int32 index)
{
	
	BeCam_Item *item = (BeCam_Item*)(fItemList->ItemAt(index));
	if(item != NULL)
	{
		item->Deselect ();
		fSelectedItem->DrawItem (this, ItemRect (index), false);
		Invalidate();
	}
	
	fSelectedItem = NULL;
	fSelectedItemIndex = -1;
}
//
//	GridView :: Deselect From To
//
void GridView::Deselect(int32 fromIndex, int32 toIndex)
{
	BeCam_Item *item;
	for(int32 i = fromIndex;i <= toIndex;i++)
	{
		item = (BeCam_Item*)(fItemList->ItemAt(i));
		if(item != NULL)
			item->Deselect();
	}
}
//
//	GridView :: Deselect All
void GridView::DeselectAll()
{
	Deselect(0,fItemList->CountItems());
}
//
//	GridView:: Current Selection
int32	GridView::CurrentSelection(int32 index = 0)
{
	return fSelectedItemIndex;
}
//
//	GridView:: Is item selected
bool	GridView::IsItemSelected(int32 index)
{
	BeCam_Item* item = (BeCam_Item*)fItemList->ItemAt (index);
	return item->IsSelected();
}
//
//	GridView:: Make Empty
void	GridView::MakeEmpty()
{
	DeleteAllItems();
}
//
//		GridView::Get the number of selected items
int GridView::GetNumberOfSelectedItems()
{
	int count = CountItems();
	int numberOfSelectedItems = 0;
	for(int index=0;index < count;index++)
		if(IsItemSelected(index))
			numberOfSelectedItems++;
	return numberOfSelectedItems;
}
//
//		GridView::TrackItem
status_t GridView::TrackItem(list_tracking_data *data)
{
    uint32  buttons;
    BPoint  point;
    // we're going to loop as long as the mouse
    //is down and hasn't moved
    // more than kDRAG SLOP pixels
    while (1) 
    {
        // make sure window is still valid
        if (data->view->Window()->Lock()) 
        {
            data->view->GetMouse(&point, &buttons);
            data->view->Window()->Unlock();
        }
        // not?  then why bother tracking
        else
            break;
        // button up?  then don't do anything
        //if (!buttons)
            //break;
        // check to see if mouse has moved more
        // than kDRAG SLOP pixels in any direction
        if ((abs((int)(data->start.x - point.x)) > kDRAG_SLOP) || (abs((int)(data->start.y - point.y)) > kDRAG_SLOP))
        {
            // make sure window is still valid
            if (data->view->Window()->Lock()) 
            {
                BBitmap	 *drag_bits;
                BBitmap	 *src_bits;
                BMessage drag_msg(B_SIMPLE_DATA);
                BView    *offscreen_view;
                int32    index = data->view->CurrentSelection();
                BeCam_Item *item;
                // get the selected item
                item = dynamic_cast<BeCam_Item *>(data->view->ItemAt(index));
                if (item) 
                {
                    // add types 
                    drag_msg.AddString("be:types", B_FILE_MIME_TYPE);
                    //some useful information
                    drag_msg.AddInt32("handle",item->GetHandle());
                    //	add the actions
					drag_msg.AddInt32("be:actions", B_COPY_TARGET);
					drag_msg.AddString("be:clip_name", item->GetName());
                    // we can even include the item
                    //drag_msg.AddRef("entry_ref",item->Ref());
                    // get bitmap from current item
                    src_bits = item->GetThumbBitmap();
                    // make sure bitmap is valid
                    if (src_bits)
                    {
                        // create a new bitmap based on the one in the list (we
                        // can't just use the bitmap we get passed because the
                        // app server owns it after we call DragMessage, besides
                        // we wan't to create that cool semi-transparent look)
                        drag_bits = new BBitmap(src_bits->Bounds(),B_RGBA32, true);
                        // we need a view
                        // so we can draw
                        offscreen_view = new BView(drag_bits->Bounds(), "",B_FOLLOW_NONE, 0);
                        drag_bits->AddChild(offscreen_view);
                        // lock it so we can draw
                        drag_bits->Lock();
                        // fill bitmap with black
                        offscreen_view->SetHighColor(0, 0, 0, 0);
                        offscreen_view->FillRect(offscreen_view->Bounds());
                        // set the alpha level
                        offscreen_view->SetDrawingMode(B_OP_ALPHA);
                        offscreen_view->SetHighColor(0, 0, 0, 128);
                        offscreen_view->SetBlendingMode(B_CONSTANT_ALPHA,B_ALPHA_COMPOSITE);
                        // blend in bitmap
                        offscreen_view->DrawBitmap(src_bits);
                        drag_bits->Unlock();
                        // initiate drag from center
                        // of bitmap
                        data->view->DragMessage(&drag_msg, drag_bits, B_OP_ALPHA,BPoint(drag_bits->Bounds().Height()/2,drag_bits->Bounds().Width()/2 ));
                    } // endif src bits
                    else
                    {
                        // no src bitmap?
                        // then just drag a rect
                        data->view->DragMessage(&drag_msg,BRect(0, 0, B_LARGE_ICON - 1,B_LARGE_ICON - 1));
					}
                } // endif item
                data->view->Window()->Unlock();
            } // endif window lock
            break;
        } // endif drag start
        // take a breather
        snooze(10000);
    } // while button
    delete(data);
    return B_NO_ERROR;
}
//
//		GridView :: ActionCopy
void GridView::ActionCopy(BMessage *request)
{
    entry_ref directory;
    request->FindRef("directory", &directory);
  	BMessage *copyRequest = new BMessage(B_COPY_TARGET);
    copyRequest->AddRef("directory",&directory);
	Window()->PostMessage(request);
}
//
//	GridView :: Index Of
int32 GridView::IndexOf(BPoint point) const
{
	int32 rowIndex = (int32)floor ((point.y - ItemMargin()) / (ItemHeight() + ItemMargin())); 
	int32 colIndex = (int32)floor ((point.x - ItemMargin()) / (ItemWidth() + ItemMargin()));
	int32 itemIndex = colIndex + rowIndex * (CountColumns());
	return itemIndex;
}
