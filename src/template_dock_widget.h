/*
 *    Copyright 2012, 2013 Thomas Schöps
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


#ifndef _OPENORIENTEERING_TEMPLATE_DOCK_WIDGET_H_
#define _OPENORIENTEERING_TEMPLATE_DOCK_WIDGET_H_

#include <QWidget>

#include <QItemSelection>

QT_BEGIN_NAMESPACE
class QGridLayout;
class QPushButton;
class QTableWidget;
class QBoxLayout;
class QToolButton;
class QGroupBox;
QT_END_NAMESPACE

class Map;
class Template;
class MapView;
class MapEditorController;

/**
 * Widget showing the list of templates, including the map layer.
 * Allows to load templates, set their view properties and reoder them,
 * and do various other actions like adjusting template positions.
 */
class TemplateWidget : public QWidget
{
Q_OBJECT
public:
	TemplateWidget(Map* map, MapView* main_view, MapEditorController* controller, QWidget* parent = NULL);
	virtual ~TemplateWidget();
	
	void addTemplateAt(Template* new_template, int pos);
	
	static Template* showOpenTemplateDialog(QWidget* dialog_parent, MapView* main_view);
	
protected:
	virtual void resizeEvent(QResizeEvent* event);
	
protected slots:
	void newTemplate(QAction* action);
	void openTemplate();
	void deleteTemplate();
	void duplicateTemplate();
	void moveTemplateUp();
	void moveTemplateDown();
	void showHelp();
	
	void cellChange(int row, int column);
	void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
	void currentCellChange(int current_row, int current_column, int previous_row, int previous_column);
	void cellDoubleClick(int row, int column);
	void updateDeleteButtonText();
	
	void moveByHandClicked(bool checked);
	void adjustClicked(bool checked);
	void adjustWindowClosed();
	//void groupClicked();
	void positionClicked(bool checked);
	void moreActionClicked(QAction* action);
	
	void templateAdded(int pos, Template* temp);
	void templatePositionDockWidgetClosed(Template* temp);
	
private:
	void addRow(int row);
	void updateRow(int row);
	int posFromRow(int row);
	int rowFromPos(int pos);
	Template* getCurrentTemplate();
	
	void changeTemplateFile(int row);
	
	QTableWidget* template_table;
	
	// Buttons
	QWidget* list_buttons_group;
	QPushButton* delete_button;
	QPushButton* duplicate_button;
	QPushButton* move_up_button;
	QPushButton* move_down_button;
	
	QGroupBox* active_buttons_group;
	QPushButton* georef_button;
	QAction* move_by_hand_action;
	QToolButton* move_by_hand_button;
	QPushButton* adjust_button;
	//QPushButton* group_button;
	QPushButton* position_button;
	//QToolButton* more_button;
	
	bool wide_layout;
	QBoxLayout* layout;
	
	Map* map;
	MapView* main_view;
	MapEditorController* controller;
	bool react_to_changes;
};

#endif