#ifndef TOKENEDITOR_HPP
#define TOKENEDITOR_HPP

#include "GuiHelpers.hpp"
#include <Widgets/OTPWidget.hpp>

class TokenEditor : public WidgetBase
{
    Q_OBJECT

public:
    explicit TokenEditor(OTPWidget::Mode mode = OTPWidget::Mode::Edit, QWidget *parent = nullptr);
    ~TokenEditor();

    void linkTokens(std::vector<OTPToken*> tokens);

    const std::vector<OTPToken*> availableTokens() const;

signals:
    void tokensSaved();

protected:
    void closeEvent(QCloseEvent *event);

private:
    QPushButton *saveButton();

    void addNewToken();
    void addNewToken(OTPToken *token);
    void saveTokens();

    void process_otpauthURI(const std::string &uri, bool fromQr = false);

    void showImportTokensMenu();
    void showExportTokensMenu();

    void updateRow(int row);
    void deleteRow(int row);

    void setAlgorithmCbIndex(QComboBox *cb, const OTPToken::ShaAlgorithm &algo);

    void setTokenIcon(int row);
    void setTokenIcon(int row, const std::string &data);
    void removeTokenIcon(int row);

private:
    OTPWidget::Mode _mode;
    std::vector<OTPToken*> _linkedTokens;

    std::shared_ptr<QMenu> btnMenu;
    std::shared_ptr<QAction> btnDeleteIcon;

    QList<std::shared_ptr<QPushButton>> buttons;
    QList<std::shared_ptr<QPushButton>> windowControls;

    std::shared_ptr<OTPWidget> tokenEditWidget;

    std::shared_ptr<QMenu> importMenu;
    QList<std::shared_ptr<QAction>> importActions;

    std::shared_ptr<QMenu> exportMenu;
    QList<std::shared_ptr<QAction>> exportActions;
};

#endif // TOKENEDITOR_HPP