#ifndef PATTERN_EDITOR_HPP
#define PATTERN_EDITOR_HPP

#include <QFrame>
#include <QUndoStack>
#include <QEvent>
#include <memory>
#include "bamboo_tracker.hpp"
#include "configuration.hpp"
#include "gui/color_palette.hpp"

namespace Ui {
	class PatternEditor;
}

class PatternEditor : public QFrame
{
	Q_OBJECT

public:
	explicit PatternEditor(QWidget *parent = nullptr);
	~PatternEditor() override;
	void setCore(std::shared_ptr<BambooTracker> core);
	void setCommandStack(std::weak_ptr<QUndoStack> stack);
	void setConfiguration(std::shared_ptr<Configuration> config);
	void setColorPallete(std::shared_ptr<ColorPalette> palette);
	void addActionToPanel(QAction* action);

	void changeEditable();
	void updatePositionByStepUpdate(bool isFirstUpdate);
	void updatepositionByPositionJump(bool trackChanged = false);
	void changeMarker();

	void copySelectedCells();
	void cutSelectedCells();

	void freeze();
	void unfreeze();

	QString getHeaderFont() const;
	int getHeaderFontSize() const;
	QString getRowsFont() const;
	int getRowsFontSize() const;
	void setFonts(QString headerFont, int headerSize, QString rowsFont, int rowsSize);

	void setHorizontalScrollMode(bool cellBased, bool refresh = true);
	void setVisibleTracks(std::vector<int> tracks);
	std::vector<int> getVisibleTracks() const;

signals:
	void currentTrackChanged(int idx);
	void currentOrderChanged(int num, int max);

	void focusIn();
	void focusOut();
	void selected(bool isSelected);
	void instrumentEntered(int num);
	void volumeEntered(int volume);
	void effectEntered(QString text);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void resizeEvent(QResizeEvent* event) override;

public slots:
	void onOrderListCurrentTrackChanged(int idx);
	void onOrderListCrrentOrderChanged(int num);

	void onOrderListEdited();
	void onDefaultPatternSizeChanged();
	void onShortcutUpdated();

	void onPatternDataGlobalChanged();

	void setPatternHighlight1Count(int count);
	void setPatternHighlight2Count(int count);
	void setEditableStep(int n);

	void onSongLoaded();

	void onDeletePressed();
	void onPastePressed();
	void onPasteMixPressed();
	void onPasteInsertPressed();
	void onPasteOverwritePressed();
	/// 0: None
	/// 1: All
	/// 2: Row
	/// 3: Column
	/// 4: Pattern
	/// 5: Order
	void onSelectPressed(int type);
	void onTransposePressed(bool isOctave, bool isIncreased);
	void onChangeValuesPressed(bool isCoarse, bool isIncreased);
	void onToggleTrackPressed();
	void onSoloTrackPressed();
	void onExpandPressed();
	void onShrinkPressed();
	void onInterpolatePressed();
	void onReversePressed();
	void onReplaceInstrumentPressed();
	void onFollowModeChanged();
	void onStoppedPlaySong();
	void onDuplicateInstrumentsRemoved();
	void onPlayStepPressed();

private:
	Ui::PatternEditor *ui;
	std::shared_ptr<BambooTracker> bt_;

	bool freezed_;
	bool songLoaded_;

	bool hScrollCellMove_;
	void updateHorizontalSliderMaximum();
};

#endif // PATTERN_EDITOR_HPP
