/*
 *    Copyright 2012-2014 Thomas Schöps
 *    Copyright 2013-2017 Kai Pastor
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "tool_base.h"

#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>

#include "gui/main_window.h"
#include "gui/widgets/key_button_bar.h"
#include "core/map.h"
#include "gui/map/map_editor.h"
#include "undo/object_undo.h"
#include "gui/map/map_widget.h"
#include "core/objects/object.h"
#include "core/objects/text_object.h"
#include "settings.h"
#include "tool_helpers.h"
#include "util/util.h"


// ### MapEditorToolBase::EditedItem ###

MapEditorToolBase::EditedItem::EditedItem(Object* original)
: active_object { original }
, duplicate     { original ? original->duplicate() : nullptr }
{
	// nothing else
	
	// These assertion must be within namespace MapEditorToolBase,
	// because EditedItem is a private member of this namespace.
	static_assert(std::is_nothrow_move_constructible<MapEditorToolBase::EditedItem>::value,
	              "MapEditorToolBase::EditedItem must be nothrow move constructible.");
	static_assert(std::is_nothrow_move_assignable<MapEditorToolBase::EditedItem>::value,
	              "MapEditorToolBase::EditedItem must be nothrow move assignable.");
}


MapEditorToolBase::EditedItem::EditedItem(const EditedItem& prototype)
: MapEditorToolBase::EditedItem::EditedItem { prototype.active_object }
{
	// nothing else
}


MapEditorToolBase::EditedItem::EditedItem(EditedItem&& prototype) noexcept
: active_object { prototype.active_object }
, duplicate     { std::move(prototype.duplicate) }
{
	// nothing else
}



MapEditorToolBase::EditedItem& MapEditorToolBase::EditedItem::operator=(const EditedItem& prototype)
{
	active_object = prototype.active_object;
	duplicate.reset(active_object ? active_object->duplicate() : nullptr);
	return *this;
}


MapEditorToolBase::EditedItem& MapEditorToolBase::EditedItem::operator=(EditedItem&& prototype) noexcept
{
	active_object = prototype.active_object;
	duplicate = std::move(prototype.duplicate);
	return *this;
}



bool MapEditorToolBase::EditedItem::isModified() const
{
	return !duplicate->equals(active_object, false);
}



// ### MapEditorToolBase ###

MapEditorToolBase::MapEditorToolBase(const QCursor& cursor, MapEditorTool::Type type, MapEditorController* editor, QAction* tool_button)
: MapEditorTool(editor, type, tool_button),
  start_drag_distance(Settings::getInstance().getStartDragDistancePx()),
  angle_helper(new ConstrainAngleToolHelper()),
  snap_helper(new SnappingToolHelper(this)),
  snap_exclude_object(nullptr),
  cur_map_widget(editor->getMainWidget()),
  key_button_bar(nullptr),
  cursor(scaledToScreen(cursor)),
  preview_update_triggered(false),
  dragging(false),
  dragging_canceled(false),
  renderables(new MapRenderables(map())),
  old_renderables(new MapRenderables(map()))
{
	angle_helper->setActive(false);
}

MapEditorToolBase::~MapEditorToolBase()
{
	old_renderables->clear(false);
	if (key_button_bar)
		editor->deletePopupWidget(key_button_bar);
}

void MapEditorToolBase::init()
{
	connect(map(), &Map::objectSelectionChanged, this, &MapEditorToolBase::objectSelectionChanged);
	connect(map(), &Map::selectedObjectEdited, this, &MapEditorToolBase::updateDirtyRect);
	initImpl();
	updateDirtyRect();
	updateStatusText();
	
	MapEditorTool::init();
}

void MapEditorToolBase::initImpl()
{
	// nothing
}

const QCursor& MapEditorToolBase::getCursor() const
{
	return cursor;
}



Qt::KeyboardModifiers MapEditorToolBase::keyButtonBarModifiers() const
{
	return Qt::KeyboardModifiers(key_button_bar ? key_button_bar->activeModifiers() : 0);
}



void MapEditorToolBase::mousePositionEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	active_modifiers = event->modifiers() | keyButtonBarModifiers();
	cur_pos = event->pos();
	cur_pos_map = map_coord;
	cur_map_widget = widget;
	updateConstrainedPositions();
	
	if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonPress)
	{
		click_pos = cur_pos;
		click_pos_map = cur_pos_map;
		constrained_click_pos = constrained_pos;
		constrained_click_pos_map = constrained_pos_map;
	}
	else if (dragging_canceled)
	{
		click_pos = cur_pos;
		click_pos_map = cur_pos_map;
		constrained_click_pos = constrained_pos;
		constrained_click_pos_map = constrained_pos_map;
		dragging_canceled = false;
	}
}


bool MapEditorToolBase::mousePressEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	mousePositionEvent(event, map_coord, widget);
	
	if (event->button() == Qt::LeftButton)
	{
		clickPress();
		return true;
	}
	else if (event->button() == Qt::RightButton)
	{
		// Do not show the ring menu when editing
		return editingInProgress();
	}
	else
	{
		return false;
	}
}

bool MapEditorToolBase::mouseMoveEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	mousePositionEvent(event, map_coord, widget);
	
	if (event->buttons().testFlag(Qt::LeftButton))
	{
		if (dragging)
		{
			updateDragging();
		}
		else if ((cur_pos - click_pos).manhattanLength() >= start_drag_distance)
		{
			startDragging();
		}
		return true;
	}
	else
	{
		mouseMove();
		return false;
	}
}

bool MapEditorToolBase::mouseReleaseEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	mousePositionEvent(event, map_coord, widget);
	
	if (event->button() == Qt::LeftButton)
	{
		if (dragging)
		{
			finishDragging();
		}
		else
		{
			clickRelease();
		}
		return true;
	}
	else if (event->button() == Qt::RightButton)
	{
		// Do not show the ring menu when editing
		return editingInProgress();
	}
	else
	{
		return false;
	}
}

bool MapEditorToolBase::keyPressEvent(QKeyEvent* event)
{
	active_modifiers = event->modifiers() | keyButtonBarModifiers();
#if defined(Q_OS_MACOS)
	// FIXME: On Mac, QKeyEvent::modifiers() seems to return the keyboard 
	// modifier flags that existed immediately before the event occurred.
	// This is in contradiction to the documenation for QKeyEvent::modifiers(),
	// but the documented behaviour of (parent) QInputEvent::modifiers().
	// Qt5 doc says QKeyEvent::modifiers() "cannot always be trusted." ...
	switch (event->key())
	{
		case Qt::Key_Shift:
			active_modifiers |= Qt::ShiftModifier;
			break;
		case Qt::Key_Control:
			active_modifiers |= Qt::ControlModifier;
			break;
		case Qt::Key_Alt:
			active_modifiers |= Qt::AltModifier;
			break;
		case Qt::Key_Meta:
			active_modifiers |= Qt::MetaModifier;
			break;
		default:
			; // nothing
	}
#endif
	return keyPress(event);
}

bool MapEditorToolBase::keyReleaseEvent(QKeyEvent* event)
{
	active_modifiers = event->modifiers() | keyButtonBarModifiers();
#if defined(Q_OS_MACOS)
	// FIXME: On Mac, QKeyEvent::modifiers() seems to return the keyboard 
	// modifier flags that existed immediately before the event occurred.
	// This is in contradiction to the documenation for QKeyEvent::modifiers(),
	// but the documented behaviour of (parent) QInputEvent::modifiers().
	// Qt5 doc says QKeyEvent::modifiers() "cannot always be trusted." ...
	switch (event->key())
	{
		case Qt::Key_Shift:
			active_modifiers &= ~Qt::ShiftModifier;
			break;
		case Qt::Key_Control:
			active_modifiers &= ~Qt::ControlModifier;
			break;
		case Qt::Key_Alt:
			active_modifiers &= ~Qt::AltModifier;
			break;
		case Qt::Key_Meta:
			active_modifiers &= ~Qt::MetaModifier;
			break;
		default:
			; // nothing
	}
#endif
	return keyRelease(event);
}

void MapEditorToolBase::draw(QPainter* painter, MapWidget* widget)
{
	drawImpl(painter, widget);
	if (angle_helper->isActive())
		angle_helper->draw(painter, widget);
	if (snap_helper->getFilter() != SnappingToolHelper::NoSnapping)
		snap_helper->draw(painter, widget);
}

void MapEditorToolBase::updateDirtyRect()
{
	int pixel_border = 0;
	QRectF rect;
	
	map()->includeSelectionRect(rect);
	if (angle_helper->isActive())
	{
		angle_helper->includeDirtyRect(rect);
		pixel_border = qMax(pixel_border, angle_helper->getDisplayRadius());
	}
	if (snap_helper->getFilter() != SnappingToolHelper::NoSnapping)
	{
		snap_helper->includeDirtyRect(rect);
		pixel_border = qMax(pixel_border, snap_helper->getDisplayRadius());
	}
	
	pixel_border = qMax(pixel_border, updateDirtyRectImpl(rect));
	if (pixel_border >= 0)
		map()->setDrawingBoundingBox(rect, pixel_border, true);
	else
		map()->clearDrawingBoundingBox();
}

void MapEditorToolBase::objectSelectionChanged()
{
	objectSelectionChangedImpl();
}

void MapEditorToolBase::drawImpl(QPainter* painter, MapWidget* widget)
{
	drawSelectionOrPreviewObjects(painter, widget);
}

void MapEditorToolBase::clickPress()
{
	// nothing
}

void MapEditorToolBase::clickRelease()
{
	// nothing
}

void MapEditorToolBase::mouseMove()
{
	// nothing
}

void MapEditorToolBase::gestureStarted()
{
	if (dragging)
		cancelDragging();
}

void MapEditorToolBase::startDragging() 
{
	Q_ASSERT(!dragging);
	dragging = true;
	dragging_canceled = false;
	dragStart();
	dragMove();
}

void MapEditorToolBase::updateDragging()
{
	Q_ASSERT(dragging);
	dragMove();
}

void MapEditorToolBase::finishDragging()
{
	Q_ASSERT(dragging);
	dragMove();
	dragging = false;
	dragFinish();
}

void MapEditorToolBase::cancelDragging()
{
	Q_ASSERT(dragging);
	dragging = false;
	dragging_canceled = true;
	dragCanceled();
}

void MapEditorToolBase::dragStart() 
{
	// nothing
}

void MapEditorToolBase::dragMove()
{
	// nothing
}

void MapEditorToolBase::dragFinish()
{
	// nothing
}

void MapEditorToolBase::dragCanceled()
{
	// nothing
}

bool MapEditorToolBase::keyPress(QKeyEvent* event)
{
	Q_UNUSED(event);
	return false;
}

bool MapEditorToolBase::keyRelease(QKeyEvent* event)
{
	Q_UNUSED(event);
	return false;
}

void MapEditorToolBase::updatePreviewObjectsSlot()
{
	preview_update_triggered = false;
	if (editingInProgress())
		updatePreviewObjects();
}

int MapEditorToolBase::updateDirtyRectImpl(QRectF& rect)
{
	Q_UNUSED(rect);
	return -1;
}

void MapEditorToolBase::updatePreviewObjects()
{
	if (!editingInProgress())
	{
		qWarning("MapEditorToolBase::updatePreviewObjects() called but editing == false");
		return;
	}
	for (auto object : editedObjects())
	{
		object->forceUpdate(); /// @todo get rid of force if possible;
		// NOTE: only necessary because of setMap(nullptr) in startEditing(..)
		renderables->insertRenderablesOfObject(object);
	}
	updateDirtyRect();
}

void MapEditorToolBase::updatePreviewObjectsAsynchronously()
{
	if (!editingInProgress())
	{
		qWarning("MapEditorToolBase::updatePreviewObjectsAsynchronously() called but editing == false");
		return;
	}
	
	if (!preview_update_triggered)
	{
		QTimer::singleShot(10, this, SLOT(updatePreviewObjectsSlot()));
		preview_update_triggered = true;
	}
}

void MapEditorToolBase::drawSelectionOrPreviewObjects(QPainter* painter, MapWidget* widget, bool draw_opaque)
{
	map()->drawSelection(painter, true, widget, renderables->empty() ? nullptr : renderables.data(), draw_opaque);
}


void MapEditorToolBase::startEditing()
{
	Q_ASSERT(!editingInProgress());
	setEditingInProgress(true);
}


void MapEditorToolBase::startEditing(Object* object)
{
	Q_ASSERT(!editingInProgress());
	setEditingInProgress(true);
	
	Q_ASSERT(object);
	if (Q_UNLIKELY(!object))
	{
		qWarning("MapEditorToolBase::startEditing(Object* object) called with object == nullptr");
		return;
	}
	
	Q_ASSERT(edited_items.empty());
	edited_items.reserve(1);
	edited_items.emplace_back(object);
	
	object->setMap(nullptr); // This is to keep the renderables out of the normal map.
	
	// Cache old renderables until the object is inserted into the map again
	old_renderables->insertRenderablesOfObject(object);
	object->takeRenderables();
}


void MapEditorToolBase::startEditing(const std::set<Object*>& objects)
{
	Q_ASSERT(!editingInProgress());
	setEditingInProgress(true);
	
	Q_ASSERT(edited_items.empty());
	edited_items.reserve(objects.size());
	for (auto object : objects)
	{
		edited_items.emplace_back(object);
		
		object->setMap(nullptr); // This is to keep the renderables out of the normal map.
		
		// Cache old renderables until the object is inserted into the map again
		old_renderables->insertRenderablesOfObject(object);
		object->takeRenderables();
	}
}

void MapEditorToolBase::abortEditing()
{
	Q_ASSERT(editingInProgress());
	
	for (auto& edited_item : edited_items)
	{
		auto object = edited_item.active_object;
		*object = *edited_item.duplicate;
		object->setMap(map());
		object->update();
	}
	edited_items.clear();
	renderables->clear();
	old_renderables->clear(true);
	MapEditorTool::setEditingInProgress(false);
}

// virtual
void MapEditorToolBase::finishEditing()
{
	Q_ASSERT(editingInProgress());
	
	if (!edited_items.empty())
	{
		auto undo_step = new ReplaceObjectsUndoStep(map());
		for (auto& edited_item : edited_items)
		{
			auto object = edited_item.active_object;
			object->setMap(map());
			object->update();
			undo_step->addObject(object, edited_item.duplicate.release());
		}
		edited_items.clear();
		map()->push(undo_step);
	}
	renderables->clear();
	old_renderables->clear(true);
	
	MapEditorTool::finishEditing();
	map()->setObjectsDirty();
	map()->emitSelectionEdited();
}


bool MapEditorToolBase::editedObjectsModified() const
{
	Q_ASSERT(editingInProgress());
	
	return std::any_of(begin(edited_items), end(edited_items), [](const EditedItem& item) {
		return item.isModified(); 
	});
}


void MapEditorToolBase::resetEditedObjects()
{
	Q_ASSERT(editingInProgress());
	
	for (auto& edited_item : edited_items)
	{
		auto object = edited_item.active_object;
		*object = *edited_item.duplicate;
		object->setMap(nullptr); // This is to keep the renderables out of the normal map.
	}
}


void MapEditorToolBase::reapplyConstraintHelpers()
{
	updateConstrainedPositions();
	if (dragging)
		dragMove();
	else
		mouseMove();
}


void MapEditorToolBase::activateAngleHelperWhileEditing(bool enable)
{
	angle_helper->setActive(enable);
	reapplyConstraintHelpers();
}

void MapEditorToolBase::activateSnapHelperWhileEditing(bool enable)
{
	snap_helper->setFilter(enable ? SnappingToolHelper::AllTypes : SnappingToolHelper::NoSnapping);
	reapplyConstraintHelpers();
}


void MapEditorToolBase::updateConstrainedPositions()
{
	if (snap_helper->getFilter() != SnappingToolHelper::NoSnapping)
	{
		SnappingToolHelperSnapInfo info;
		constrained_pos_map = MapCoordF(snap_helper->snapToObject(cur_pos_map, cur_map_widget, &info, snap_exclude_object));
		constrained_pos = cur_map_widget->mapToViewport(constrained_pos_map);
		snapped_to_pos = info.type != SnappingToolHelper::NoSnapping;
	}
	else
	{
		constrained_pos_map = cur_pos_map;
		constrained_pos = cur_pos;
		snapped_to_pos = false;
	}
	
	if (angle_helper->isActive())
	{
		angle_helper->getConstrainedCursorPositions(constrained_pos_map, constrained_pos_map, constrained_pos, cur_map_widget);
	}
}
