// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Slimecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "slimecoinamountfield.h"

#include "slimecoinunits.h"
#include "guiconstants.h"
#include "qvaluecombobox.h"
#include "platformstyle.h"

#include <QDebug>
#include <QApplication>
#include <QAbstractSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define QTversionPreFiveEleven
#endif

/** QSpinBox that uses fixed-point numbers internally and uses our own
 * formatting/parsing functions.
 */
class AmountSpinBox: public QAbstractSpinBox
{
    Q_OBJECT

public:
    explicit AmountSpinBox(QWidget *parent):
        QAbstractSpinBox(parent),
        currentUnit(SlimecoinUnits::SLME),
        singleStep(100000), // satoshis
        assetUnit(-1)
    {
        setAlignment(Qt::AlignRight);

        connect(lineEdit(), SIGNAL(textEdited(QString)), this, SIGNAL(valueChanged()));
    }

    QValidator::State validate(QString &text, int &pos) const
    {
        if(text.isEmpty())
            return QValidator::Intermediate;
        bool valid = false;
        parse(text, &valid);
        /* Make sure we return Intermediate so that fixup() is called on defocus */
        return valid ? QValidator::Intermediate : QValidator::Invalid;
    }

    void fixup(QString &input) const
    {
        bool valid = false;
        CAmount val = parse(input, &valid);
        if(valid)
        {
            input = SlimecoinUnits::format(currentUnit, val, false, SlimecoinUnits::separatorAlways, assetUnit);
            lineEdit()->setText(input);
        }
    }

    CAmount value(bool *valid_out=0) const
    {
        return parse(text(), valid_out);
    }

    void setValue(const CAmount& value)
    {
        lineEdit()->setText(SlimecoinUnits::format(currentUnit, value, false, SlimecoinUnits::separatorAlways, assetUnit));
        Q_EMIT valueChanged();
    }

    void stepBy(int steps)
    {
        bool valid = false;
        CAmount val = value(&valid);
        val = val + steps * singleStep;
        val = qMin(qMax(val, CAmount(0)), SlimecoinUnits::maxMoney());
        setValue(val);
    }

    void setDisplayUnit(int unit)
    {
        bool valid = false;
        CAmount val = value(&valid);

        currentUnit = unit;

        if(valid)
            setValue(val);
        else
            clear();
    }

    void setSingleStep(const CAmount& step)
    {
        singleStep = step;
    }

    void setAssetUnit(int unit)
    {
        if (unit > MAX_ASSET_UNITS)
            unit = MAX_ASSET_UNITS;

        assetUnit = unit;

        bool valid = false;
        CAmount val = value(&valid);

        if(valid)
            setValue(val);
        else
            clear();
    }

    QSize minimumSizeHint() const
    {
        if(cachedMinimumSizeHint.isEmpty())
        {
            ensurePolished();

            const QFontMetrics fm(fontMetrics());
            int h = lineEdit()->minimumSizeHint().height();
			#ifndef QTversionPreFiveEleven
            	int w = fm.horizontalAdvance(SlimecoinUnits::format(SlimecoinUnits::SLME, SlimecoinUnits::maxMoney(), false, SlimecoinUnits::separatorAlways, assetUnit));
			#else
				int w = fm.width(SlimecoinUnits::format(SlimecoinUnits::SLME, SlimecoinUnits::maxMoney(), false, SlimecoinUnits::separatorAlways, assetUnit));
			#endif
            w += 2; // cursor blinking space

            QStyleOptionSpinBox opt;
            initStyleOption(&opt);
            QSize hint(w, h);
            QSize extra(35, 6);
            opt.rect.setSize(hint + extra);
            extra += hint - style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                                    QStyle::SC_SpinBoxEditField, this).size();
            // get closer to final result by repeating the calculation
            opt.rect.setSize(hint + extra);
            extra += hint - style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                                    QStyle::SC_SpinBoxEditField, this).size();
            hint += extra;
            hint.setHeight(h);

            opt.rect = rect();

            cachedMinimumSizeHint = style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, this)
                                    .expandedTo(QApplication::globalStrut());
        }
        return cachedMinimumSizeHint;
    }

private:
    int currentUnit;
    CAmount singleStep;
    mutable QSize cachedMinimumSizeHint;
    int assetUnit;

    /**
     * Parse a string into a number of base monetary units and
     * return validity.
     * @note Must return 0 if !valid.
     */
    CAmount parse(const QString &text, bool *valid_out=0) const
    {
        CAmount val = 0;

        // Update parsing function to work with asset parsing units
        bool valid = false;
        if (assetUnit >= 0) {
            valid = SlimecoinUnits::assetParse(assetUnit, text, &val);
        }
        else
            valid = SlimecoinUnits::parse(currentUnit, text, &val);

        if(valid)
        {
            if(val < 0 || val > SlimecoinUnits::maxMoney())
                valid = false;
        }
        if(valid_out)
            *valid_out = valid;
        return valid ? val : 0;
    }

protected:
    bool event(QEvent *event)
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Comma)
            {
                // Translate a comma into a period
                QKeyEvent periodKeyEvent(event->type(), Qt::Key_Period, keyEvent->modifiers(), ".", keyEvent->isAutoRepeat(), keyEvent->count());
                return QAbstractSpinBox::event(&periodKeyEvent);
            }
        }
        return QAbstractSpinBox::event(event);
    }

    StepEnabled stepEnabled() const
    {
        if (isReadOnly()) // Disable steps when AmountSpinBox is read-only
            return StepNone;
        if (text().isEmpty()) // Allow step-up with empty field
            return StepUpEnabled;

        StepEnabled rv = StepNone;
        bool valid = false;
        CAmount val = value(&valid);
        if(valid)
        {
            if(val > 0)
                rv |= StepDownEnabled;
            if(val < SlimecoinUnits::maxMoney())
                rv |= StepUpEnabled;
        }
        return rv;
    }

Q_SIGNALS:
    void valueChanged();
};

#include "slimecoinamountfield.moc"

SlimecoinAmountField::SlimecoinAmountField(QWidget *parent) :
    QWidget(parent),
    amount(0)
{
    amount = new AmountSpinBox(this);
    amount->setLocale(QLocale::c());
    amount->installEventFilter(this);
    amount->setMaximumWidth(170);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(amount);
    unit = new QValueComboBox();
    unit->setModel(new SlimecoinUnits(this));
    layout->addWidget(unit);
    layout->addStretch(1);
    layout->setContentsMargins(0,0,0,0);

    setLayout(layout);

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(amount);

    // If one if the widgets changes, the combined content changes as well
    connect(amount, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
    connect(unit, SIGNAL(currentIndexChanged(int)), this, SLOT(unitChanged(int)));

    // Set default based on configuration
    unitChanged(unit->currentIndex());

}

void SlimecoinAmountField::clear()
{
    amount->clear();
    unit->setCurrentIndex(0);
}

void SlimecoinAmountField::setEnabled(bool fEnabled)
{
    amount->setEnabled(fEnabled);
    unit->setEnabled(fEnabled);
}

bool SlimecoinAmountField::validate()
{
    bool valid = false;
    value(&valid);
    setValid(valid);
    return valid;
}

void SlimecoinAmountField::setValid(bool valid)
{
    if (valid) {
            amount->setStyleSheet("");
    } else {
            amount->setStyleSheet(STYLE_INVALID);
    }
}

bool SlimecoinAmountField::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        // Clear invalid flag on focus
        setValid(true);
    }
    return QWidget::eventFilter(object, event);
}

QWidget *SlimecoinAmountField::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, amount);
    QWidget::setTabOrder(amount, unit);
    return unit;
}

CAmount SlimecoinAmountField::value(bool *valid_out) const
{
    return amount->value(valid_out);
}

void SlimecoinAmountField::setValue(const CAmount& value)
{
    amount->setValue(value);
}

void SlimecoinAmountField::setReadOnly(bool fReadOnly)
{
    amount->setReadOnly(fReadOnly);
}

void SlimecoinAmountField::unitChanged(int idx)
{
    // Use description tooltip for current unit for the combobox
    unit->setToolTip(unit->itemData(idx, Qt::ToolTipRole).toString());

    // Determine new unit ID
    int newUnit = unit->itemData(idx, SlimecoinUnits::UnitRole).toInt();

    amount->setDisplayUnit(newUnit);
}

void SlimecoinAmountField::setDisplayUnit(int newUnit)
{
    unit->setValue(newUnit);
}

void SlimecoinAmountField::setSingleStep(const CAmount& step)
{
    amount->setSingleStep(step);
}

AssetAmountField::AssetAmountField(QWidget *parent) :
        QWidget(parent),
        amount(0)
{
    amount = new AmountSpinBox(this);
    amount->setLocale(QLocale::c());
    amount->installEventFilter(this);
    amount->setMaximumWidth(170);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(amount);
    layout->addStretch(1);
    layout->setContentsMargins(0,0,0,0);

    setLayout(layout);

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(amount);

    // If one if the widgets changes, the combined content changes as well
    connect(amount, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));

    // Set default based on configuration
    setUnit(MAX_ASSET_UNITS);
}

void AssetAmountField::clear()
{
    amount->clear();
    setUnit(MAX_ASSET_UNITS);
}

void AssetAmountField::setEnabled(bool fEnabled)
{
    amount->setEnabled(fEnabled);
}

bool AssetAmountField::validate()
{
    bool valid = false;
    value(&valid);
    setValid(valid);
    return valid;
}

void AssetAmountField::setValid(bool valid)
{
    if (valid) {
        amount->setStyleSheet("");
    } else {
        amount->setStyleSheet(STYLE_INVALID);
    }
}

bool AssetAmountField::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        // Clear invalid flag on focus
        setValid(true);
    }
    return QWidget::eventFilter(object, event);
}

CAmount AssetAmountField::value(bool *valid_out) const
{
    return amount->value(valid_out) * SlimecoinUnits::factorAsset(8 - assetUnit);
}

void AssetAmountField::setValue(const CAmount& value)
{
    amount->setValue(value);
}

void AssetAmountField::setReadOnly(bool fReadOnly)
{
    amount->setReadOnly(fReadOnly);
}

void AssetAmountField::setSingleStep(const CAmount& step)
{
    amount->setSingleStep(step);
}

void AssetAmountField::setUnit(int unit)
{
    assetUnit = unit;
    amount->setAssetUnit(assetUnit);
}
