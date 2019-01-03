#ifndef LABELED_VERTICAL_SLIDER_HPP
#define LABELED_VERTICAL_SLIDER_HPP

#include <QFrame>
#include <QString>
#include <QSlider>
#include <QEvent>

namespace Ui {
	class LabeledVerticalSlider;
}

class LabeledVerticalSlider : public QFrame
{
	Q_OBJECT

public:
	explicit LabeledVerticalSlider(QWidget *parent = nullptr);
	LabeledVerticalSlider(QString text, QString prefix = "", QString suffix = "", QWidget *parent = nullptr);
	~LabeledVerticalSlider() override;

	int value() const;
	void setValue(int value);

	int maximum() const;
	void setMaximum(int value);
	int minimum() const;
	void setMinimum(int value);

	void setValueRate(double rate, int precision = 1);
	void setSign(bool enabled);

	void setTickPosition(QSlider::TickPosition position);
	void setTickInterval(int ti);

	QString text() const;
	void setText(QString text);
	QString suffix() const;
	void setSuffix(QString suffix);
	QString prefix() const;
	void setprefix(QString prefix);

signals:
	void valueChanged(int value);

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
	void on_slider_valueChanged(int value);

private:
	Ui::LabeledVerticalSlider *ui;
	QString suffix_, prefix_;
	double rate_;
	int precision_;
	bool isSigned_;

	void init(QString text, QString prefix, QString suffix);
	void updateValueLabel();
};

#endif // LABELED_VERTICAL_SLIDER_HPP
