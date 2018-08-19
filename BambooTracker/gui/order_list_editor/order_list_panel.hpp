#ifndef ORDER_LIST_PANEL_HPP
#define ORDER_LIST_PANEL_HPP

#include <QWidget>
#include <QPixmap>
#include <QFont>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QWheelEvent>
#include <QEvent>
#include <QRect>
#include <QColor>
#include <memory>
#include "bamboo_tracker.hpp"
#include "gui/order_list_editor/order_position.hpp"
#include "module.hpp"

class OrderListPanel : public QWidget
{
	Q_OBJECT
public:
	explicit OrderListPanel(QWidget *parent = nullptr);
	void setCore(std::shared_ptr<BambooTracker> core);

	void changeEditable();

public slots:
	void setCurrentTrackForSlider(int num);
	void setCurrentOrderForSlider(int num);
	void setCurrentTrack(int num);
	void setCurrentOrder(int num);

signals:
	void currentTrackChangedForSlider(int num);
	void currentOrderChangedForSlider(int num);
	void currentTrackChanged(int num);
	void currentOrderChanged(int num);

protected:
	virtual bool event(QEvent *event) override;
	bool keyPressed(QKeyEvent* event);
	bool keyReleased(QKeyEvent* event);
	virtual void paintEvent(QPaintEvent* event) override;
	virtual void resizeEvent(QResizeEvent* event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void mouseReleaseEvent(QMouseEvent* event) override;
	bool mouseHoverd(QHoverEvent* event);
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void leaveEvent(QEvent* event) override;

private:
	std::unique_ptr<QPixmap> pixmap_;
	std::shared_ptr<BambooTracker> bt_;

	QFont rowFont_, headerFont_;
	int rowFontWidth_, rowFontHeight_, rowFontAscend_, rowFontLeading_;

	int widthSpace_;
	int rowNumWidth_;
	int trackWidth_;
	int columnsWidthFromLeftToEnd_;
	int headerHeight_;
	int curRowBaselineY_;
	int curRowY_;

	QColor defTextColor_, defRowColor_;
	QColor curTextColor_, curRowColor_, curRowColorEditable_, curCellColor_;
	QColor selCellColor_;
	QColor hovCellColor_;
	QColor rowNumColor_;
	QColor headerTextColor_, headerRowColor_;
	QColor borderColor_;

	int leftTrackNum_;
	ModuleStyle modStyle_;

	int curSongNum_;
	OrderPosition curPos_, hovPos_;
	OrderPosition mousePressPos_, mouseReleasePos_;
	OrderPosition selLeftAbovePos_, selRightBelowPos_;
	OrderPosition shiftPressedPos_;

	bool isIgnoreToSlider_, isIgnoreToPattern_;

	void initDisplay();

	void drawList(const QRect& rect);
	void drawRows(int maxWidth);
	void drawHeaders(int maxWidth);
	void drawBorders(int maxWidth);
	void drawShadow();

	int calculateColumnsWidthWithRowNum(int begin, int end) const;

	void moveCursorToRight(int n);
	void moveCursorToDown(int n);

	void setSelectedRectangle(const OrderPosition& start, const OrderPosition& end);
	bool isSelectedCell(int track, int row);
};

#endif // ORDER_LIST_PANEL_HPP
