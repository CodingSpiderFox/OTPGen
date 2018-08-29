#include "TokenEditor.hpp"

#include <QFileDialog>
#include <QMessageBox>
#include <QImageReader>

#include <QFile>

#include "UserInputDialog.hpp"

#include <TokenDatabase.hpp>
#include <AppSupport.hpp>

#include "GuiConfig.hpp"

#ifdef OTPGEN_WITH_QR_CODES
#include <Tools/QRCode.hpp>
#endif

#include <Tools/otpauthURI.hpp>

TokenEditor::TokenEditor(OTPWidget::Mode mode, QWidget *parent)
    : WidgetBase(parent),
      _mode(mode)
{
    this->setWindowFlag(Qt::FramelessWindowHint, true);
    this->setPalette(GuiHelpers::make_theme(this->palette()));
    if (mode == OTPWidget::Mode::Edit)
    {
        this->setWindowTitle(GuiHelpers::make_windowTitle("Add Tokens"));
    }
    else if (mode == OTPWidget::Mode::Override)
    {
        this->setWindowTitle(GuiHelpers::make_windowTitle("Edit Tokens"));
    }
    else
    {
        this->setWindowTitle(GuiHelpers::make_windowTitle("Export Tokens"));
    }
    this->setWindowIcon(static_cast<AppIcon*>(qApp->userData(0))->icon);

    // initial window size
    this->resize(gcfg::defaultGeometryTokenEditor());

    GuiHelpers::centerWindow(this);

    vbox = GuiHelpers::make_vbox();

    if (mode == OTPWidget::Mode::Edit) {

    importMenu = std::make_shared<QMenu>();
    importActions = {
        GuiHelpers::make_menuAction("andOTP", QIcon(":/logos/andotp.svgz"), this, [&]{
            std::vector<OTPToken*> target;
            auto file = QFileDialog::getOpenFileName(this, "Open andOTP token file", QString(),
                "otp_accounts.json (Plain Text) (otp_accounts.json);;"
                "otp_accounts.json.aes (Encrypted) (otp_accounts.json.aes)");
            if (file.isEmpty() || file.isNull())
            {
                return;
            }

            bool status;
            AppSupport::andOTP::Type type;
            std::string password;
            bool hasPassword = false;

            if (file.endsWith("json", Qt::CaseInsensitive))
            {
                type = AppSupport::andOTP::PlainText;

            }
            else
            {
                type = AppSupport::andOTP::Encrypted;

                auto passwordDialog = std::make_shared<UserInputDialog>(UserInputDialog::Password);
                passwordDialog->setDialogNotice("Please enter the decryption password for the given andOTP token database.");
                QObject::connect(passwordDialog.get(), &UserInputDialog::textEntered, this, [&](const QString &pwd){
                    password = pwd.toUtf8().constData();
                    hasPassword = true;
                });
                passwordDialog->exec();

                if (password.empty() || !hasPassword)
                {
                    password.clear();
                    QMessageBox::critical(this, "Password missing", "You didn't entered a password for decryption. Import process will be aborted.");
                    return;
                }
            }

            status = AppSupport::andOTP::importTokens(file.toUtf8().constData(), target, type, password);
            password.clear();
            if (status)
            {
                for(auto&& t : target)
                {
                    addNewToken(t);
                    delete t;
                }
            }
            else
            {
                QMessageBox::critical(this, "Import Error",
                    QString("An error occurred during importing your andOTP tokens! "
                            "If you imported an encrypted file make sure the password is correct.\n\n"
                            "Mode: %1").arg(type == AppSupport::andOTP::PlainText ? "Plain Text Import" : "Encrypted Import"));
            }
        }),
        GuiHelpers::make_menuAction("Authy", QIcon(":/logos/authy.svgz"), this, [&]{
            auto file = QFileDialog::getOpenFileName(this, "Open Authy token file", QString(),
                "com.authy.storage.tokens.authenticator.xml (TOTP Tokens) (com.authy.storage.tokens.authenticator.xml);;"
                "com.authy.storage.tokens.authy.xml (Authy Tokens) (com.authy.storage.tokens.authy.xml);;"
                "com.authy.storage.tokens.authenticator.json (TOTP Tokens) (com.authy.storage.tokens.authenticator.json);;"
                "com.authy.storage.tokens.authy.json (Authy Tokens) (com.authy.storage.tokens.authy.json)");
            if (file.isEmpty() || file.isNull())
            {
                return;
            }

            if (file.endsWith("com.authy.storage.tokens.authenticator.xml", Qt::CaseInsensitive))
            {
                std::vector<TOTPToken> target;
                auto status = AppSupport::Authy::importTOTP(file.toUtf8().constData(), target, AppSupport::Authy::XML);
                if (status)
                {
                    for(auto&& t : target)
                    {
                        addNewToken(&t);
                    }
                }
                else
                {
                    QMessageBox::critical(this, "Import Error", "An error occurred during importing your Authy TOTP tokens!"
                                                                "\n\nFormat: XML");
                }
            }
            else if (file.endsWith("com.authy.storage.tokens.authenticator.json", Qt::CaseInsensitive))
            {
                std::vector<TOTPToken> target;
                auto status = AppSupport::Authy::importTOTP(file.toUtf8().constData(), target, AppSupport::Authy::JSON);
                if (status)
                {
                    for(auto&& t : target)
                    {
                        addNewToken(&t);
                    }
                }
                else
                {
                    QMessageBox::critical(this, "Import Error", "An error occurred during importing your Authy TOTP tokens!"
                                                                "\n\nFormat: JSON");
                }
            }
            else if (file.endsWith("com.authy.storage.tokens.authy.xml", Qt::CaseInsensitive))
            {
                std::vector<AuthyToken> target;
                auto status = AppSupport::Authy::importNative(file.toUtf8().constData(), target, AppSupport::Authy::XML);
                if (status)
                {
                    for(auto&& t : target)
                    {
                        addNewToken(&t);
                    }
                }
                else
                {
                    QMessageBox::critical(this, "Import Error", "An error occurred during importing your Authy native tokens!"
                                                                "\n\nFormat: XML");
                }
            }
            else if (file.endsWith("com.authy.storage.tokens.authy.json", Qt::CaseInsensitive))
            {
                std::vector<AuthyToken> target;
                auto status = AppSupport::Authy::importNative(file.toUtf8().constData(), target, AppSupport::Authy::JSON);
                if (status)
                {
                    for(auto&& t : target)
                    {
                        addNewToken(&t);
                    }
                }
                else
                {
                    QMessageBox::critical(this, "Import Error", "An error occurred during importing your Authy native tokens!"
                                                                "\n\nFormat: JSON");
                }
            }
            else
            {
                QMessageBox::critical(this, "Unknown Authy file", "The given file wasn't recognized as an Authy token file.");
            }
        }),
        GuiHelpers::make_menuAction("Steam", QIcon(":/logos/steam.svgz"), this, [&]{
            SteamToken token("Steam");
            auto file = QFileDialog::getOpenFileName(this, "Open SteamGuard configuration file", QString(),
                "Steamguard* (Steamguard*)");
            if (file.isEmpty() || file.isNull())
            {
                return;
            }

            auto status = AppSupport::Steam::importFromSteamGuard(file.toUtf8().constData(), token);
            if (status)
            {
                addNewToken(&token);
            }
            else
            {
                QMessageBox::critical(this, "Import Error", "An error occurred during importing your Steam token!");
            }
        }),
        GuiHelpers::make_menuSeparator(),
    #ifdef OTPGEN_WITH_QR_CODES
        GuiHelpers::make_menuAction("QR Code", GuiHelpers::i()->qr_code_icon(), this, [&]{
            auto file = QFileDialog::getOpenFileName(this, "Open QR Code", QString(),
                "PNG, JPG, SVG (*.png *.jpg *.jpe *.jpeg *.svg)");
            if (file.isEmpty() || file.isNull())
            {
                return;
            }
            if (!(file.endsWith("png", Qt::CaseInsensitive) ||
                file.endsWith("jpg", Qt::CaseInsensitive) ||
                file.endsWith("jpe", Qt::CaseInsensitive) ||
                file.endsWith("jpeg", Qt::CaseInsensitive) ||
                file.endsWith("svg", Qt::CaseInsensitive)))
            {
                QMessageBox::critical(this, "Unsupported Image Format",
                    "This image format is not supported by the QR decoder.\n"
                    "Only PNG, JPG and SVG (experimental) are supported.");
                return;
            }

            std::string out;
            auto res = QRCode::decode(file.toUtf8().constData(), out);
            if (!res)
            {
                QMessageBox::critical(this, "QR Code Error",
                    "Failed to decode the given image. Make sure that the image is a valid QR Code and that the image quality is not too low. "
                    "Also note that the decoder currently only supports PNG, JPG and SVG (experimental).");
                return;
            }

            process_otpauthURI(out, true);
        }),
    #endif
        GuiHelpers::make_menuAction("otpauth URI", QIcon(), this, [&]{
            std::string uri;
            auto inputDialog = std::make_shared<UserInputDialog>();
            inputDialog->setDialogNotice("Please enter a otpauth:// uri.");
            QObject::connect(inputDialog.get(), &UserInputDialog::textEntered, this, [&](const QString &text){
                uri = text.toUtf8().constData();
            });
            inputDialog->exec();

            if (uri.empty())
            {
                QMessageBox::critical(this, "URI missing", "You didn't entered a otpauth:// uri. Import process will be aborted.");
                return;
            }

            process_otpauthURI(uri);
        }),
    };

    for (auto&& action : importActions)
    {
        importMenu->addAction(action.get());
    }

    buttons = GuiHelpers::make_tokenControls(this,
        true, "New token", [&]{ addNewToken(); },
        false, "", [&]{ }
    );
    buttons.append(GuiHelpers::make_toolbtn(GuiHelpers::i()->import_icon(), "Import tokens", this, [&]{
        showImportTokensMenu();
    }));

    } // Edit Mode

    else {

    exportMenu = std::make_shared<QMenu>();
    exportActions = {
        GuiHelpers::make_menuAction("andOTP", QIcon(":/logos/andotp.svgz"), this, [&]{

            const auto answer = QMessageBox::question(this, "andOTP",
                "<b>WARNING!</b><br><br>Currently only plain text exports of andOTP json files are supported!<br>"
                "Continue at your own risk. Click \"Yes\" to continue the export.");
            if (answer != QMessageBox::Yes)
            {
                return;
            }

            auto file = QFileDialog::getSaveFileName(this, "Save andOTP json");
            if (file.isEmpty() || file.isNull())
            {
                return;
            }

            const auto res = AppSupport::andOTP::exportTokens(file.toUtf8().constData(), this->availableTokens());
            if (res)
            {
                QMessageBox::information(this, "andOTP", "Successfully exported an andOTP json file!");
            }
            else
            {
                QMessageBox::critical(this, "andOTP", "Unable to export an andOTP json file!");
            }
        }),
        GuiHelpers::make_menuSeparator(),
    #ifdef OTPGEN_WITH_QR_CODES
        GuiHelpers::make_menuAction("QR Code", GuiHelpers::i()->qr_code_icon(), this, [&]{
            // TODO: export qr codes
            QMessageBox::information(this, "Not Implemented", "This feature is not implemented yet.");
        }),
    #endif
        GuiHelpers::make_menuAction("otpauth URI", QIcon(), this, [&]{
            // TODO: export otpauth uris
            QMessageBox::information(this, "Not Implemented", "This feature is not implemented yet.");
        }),
    };

    for (auto&& action : exportActions)
    {
        exportMenu->addAction(action.get());
    }

    buttons = {
        GuiHelpers::make_toolbtn(GuiHelpers::i()->export_icon(), "Export tokens", this, [&]{
            showExportTokensMenu();
        })
    };

    } // View Mode

    windowControls.append(GuiHelpers::make_toolbtn(GuiHelpers::i()->save_icon(), "Save tokens", this, [&]{
        saveTokens();
    }));
    windowControls.append(GuiHelpers::make_windowControls(this,
        false, [&]{ },
        false, [&]{ },
        true, [&]{ GuiHelpers::default_closeCallback(this); }
    ));

    if (mode == OTPWidget::Mode::Edit)
    {
        titleBar = GuiHelpers::make_titlebar("Add Tokens", buttons, windowControls);
    }
    else if (mode == OTPWidget::Mode::Override)
    {
        titleBar = GuiHelpers::make_titlebar("Edit Tokens", buttons, windowControls);
    }
    else
    {
        titleBar = GuiHelpers::make_titlebar("Export Tokens", buttons, windowControls);

        saveButton()->setDisabled(true);
        saveButton()->setVisible(false);
    }
    vbox->addWidget(titleBar.get());

    tokenEditWidget = std::make_shared<OTPWidget>(OTPWidget::Mode::Edit);
    tokenEditWidget->setContentsMargins(3,3,3,3);
    vbox->addWidget(tokenEditWidget.get());

    btnMenu = std::make_shared<QMenu>();
    btnDeleteIcon = std::make_shared<QAction>();
    btnDeleteIcon->setText("Delete Icon");
    QObject::connect(btnDeleteIcon.get(), &QAction::triggered, this, [&]{
        const auto row = static_cast<TableWidgetCellUserData*>(btnDeleteIcon->userData(0))->row;
        btnDeleteIcon->setUserData(0, nullptr);
        removeTokenIcon(row);
    });
    btnMenu->addAction(btnDeleteIcon.get());

    this->setLayout(vbox.get());

    // Restore UI state
    const auto _geometry = saveGeometry();
    restoreGeometry(gcfg::settings()->value(gcfg::keyGeometryTokenEditor(), _geometry).toByteArray());
    const auto columns = gcfg::settings()->value(gcfg::keyTokenEditWidgetColumns());
    if (!columns.isNull())
    {
        QSequentialIterable iterable = columns.value<QSequentialIterable>();
        int c = 0;
        for (auto&& col : iterable)
        {
            tokenEditWidget->tokens()->setColumnWidth(c, col.toInt());
            ++c;
        }
    }
}

TokenEditor::~TokenEditor()
{
    buttons.clear();
    windowControls.clear();
    importActions.clear();
}

QPushButton *TokenEditor::saveButton()
{
    return windowControls[0].get();
}

void TokenEditor::linkTokens(std::vector<OTPToken*> tokens)
{
    // delete previous linked tokens
    for (auto i = 0; i < tokenEditWidget->tokens()->rowCount(); i++)
    {
        deleteRow(i);
    }

    // update links
    _linkedTokens = tokens;
    for (auto&& token : tokens)
    {
        addNewToken(token);
    }

    // disable editing in export mode
    if (_mode == OTPWidget::Mode::Export)
    {
        for (auto i = 0; i < tokenEditWidget->tokens()->rowCount(); i++)
        {
            tokenEditWidget->tokens()->tokenEditType(i)->setDisabled(true);
            QObject::disconnect(tokenEditWidget->tokens()->tokenEditIcon(i), nullptr, nullptr, nullptr);
            tokenEditWidget->tokens()->tokenEditLabel(i)->setReadOnly(true);
            tokenEditWidget->tokens()->tokenEditSecret(i)->setDisabled(true);
            if (tokenEditWidget->tokens()->tokenEditSecretComboBoxExtra(i))
            {
                tokenEditWidget->tokens()->tokenEditSecretComboBoxExtra(i)->setDisabled(true);
            }
            tokenEditWidget->tokens()->tokenDigits(i)->setDisabled(true);
            tokenEditWidget->tokens()->tokenPeriod(i)->setDisabled(true);
            tokenEditWidget->tokens()->tokenCounter(i)->setDisabled(true);
            tokenEditWidget->tokens()->cellWidget(i, 6)->setDisabled(true);
        }
    }
}

const std::vector<OTPToken*> TokenEditor::availableTokens() const
{
    std::vector<OTPToken*> tokens;
    for (auto i = 0; i < tokenEditWidget->tokens()->rowCount(); i++)
    {
        tokens.emplace_back(TokenStore::i()->tokenAt(tokenEditWidget->tokens()->tokenEditLabel(i)->text().toUtf8().constData()));
    }
    return tokens;
}

void TokenEditor::closeEvent(QCloseEvent *event)
{
    // Save UI state
    gcfg::settings()->setValue(gcfg::keyGeometryTokenEditor(), saveGeometry());
    QList<int> columns;
    for (auto i = 0; i < tokenEditWidget->tokens()->columnCount(); i++)
    {
        columns.append(tokenEditWidget->tokens()->columnWidth(i));
    }
    gcfg::settings()->setValue(gcfg::keyTokenEditWidgetColumns(), QVariant::fromValue(columns));
    gcfg::settings()->sync();

    WidgetBase::closeEvent(event);
}

void TokenEditor::addNewToken()
{
    auto tokens = tokenEditWidget->tokens();
    const auto row = tokens->rowCount();

    tokens->insertRow(row);

    // row user data:
    //  - Type
    //  - Delete Button

    // setup row data
    tokens->setCellWidget(row, 0, OTPWidget::make_typeCb(row, this,
        [&](int){ updateRow(static_cast<TableWidgetCellUserData*>(this->sender()->userData(0))->row); }));
    tokens->setCellWidget(row, 1, OTPWidget::make_labelInput(row, this, [&]{ // Label
        setTokenIcon(static_cast<TableWidgetCellUserData*>(this->sender()->userData(0))->row);
    }, [&](const QPoint&){
        btnDeleteIcon->setUserData(0, this->sender()->userData(0));
        btnMenu->popup(QCursor::pos());
    }));
    tokens->setCellWidget(row, 2, OTPWidget::make_secretInput()); // Secret
    tokens->setCellWidget(row, 3, OTPWidget::make_intInput(OTPToken::min_digits, OTPToken::max_digits)); // Digits
    tokens->setCellWidget(row, 4, OTPWidget::make_intInput(OTPToken::min_period, OTPToken::max_period)); // Period
    tokens->setCellWidget(row, 5, OTPWidget::make_intInput(OTPToken::min_counter, OTPToken::max_counter)); // Counter
    tokens->setCellWidget(row, 6, OTPWidget::make_algoCb());
    tokens->setCellWidget(row, 7, OTPWidget::make_buttons(row, this,
        [&]{ deleteRow(static_cast<TableWidgetCellUserData*>(this->sender()->userData(0))->row); }));

    // update row
    updateRow(row);
}

void TokenEditor::addNewToken(OTPToken *token)
{
    auto tokens = tokenEditWidget->tokens();
    const auto row = tokens->rowCount();

    tokens->insertRow(row);

    // row user data:
    //  - Type
    //  - Delete Button

    // setup row data
    tokens->setCellWidget(row, 0, OTPWidget::make_typeCb(row, this,
        [&](int){ updateRow(static_cast<TableWidgetCellUserData*>(this->sender()->userData(0))->row); }));
    tokens->setCellWidget(row, 1, OTPWidget::make_labelInput(row, this, [&]{ // Label
        setTokenIcon(static_cast<TableWidgetCellUserData*>(this->sender()->userData(0))->row);
    }, [&](const QPoint&){
        btnDeleteIcon->setUserData(0, this->sender()->userData(0));
        btnMenu->popup(QCursor::pos());
    }));
    if (_mode == OTPWidget::Mode::Override || _mode == OTPWidget::Mode::Export)
    {
        tokens->cellWidget(row, 1)->setUserData(2, new TokenOldNameUserData(token->label()));
    }
    tokens->setCellWidget(row, 2, OTPWidget::make_secretInput()); // Secret
    tokens->setCellWidget(row, 3, OTPWidget::make_intInput(OTPToken::min_digits, OTPToken::max_digits)); // Digits
    tokens->setCellWidget(row, 4, OTPWidget::make_intInput(OTPToken::min_period, OTPToken::max_period)); // Period
    tokens->setCellWidget(row, 5, OTPWidget::make_intInput(OTPToken::min_counter, OTPToken::max_counter)); // Counter
    tokens->setCellWidget(row, 6, OTPWidget::make_algoCb());
    tokens->setCellWidget(row, 7, OTPWidget::make_buttons(row, this,
        [&]{ deleteRow(static_cast<TableWidgetCellUserData*>(this->sender()->userData(0))->row); }));

    // update row
    updateRow(row);

    // set token data
    switch (token->type())
    {
        case OTPToken::TOTP:
            tokens->tokenEditType(row)->setCurrentIndex(0);
            tokens->tokenDigits(row)->setText(QString::number(token->digits()));
            tokens->tokenPeriod(row)->setText(QString::number(token->period()));
            setAlgorithmCbIndex(tokens->tokenAlgorithm(row), token->algorithm());
            break;
        case OTPToken::HOTP:
            tokens->tokenEditType(row)->setCurrentIndex(1);
            tokens->tokenDigits(row)->setText(QString::number(token->digits()));
            tokens->tokenPeriod(row)->setText(QString::number(token->period()));
            setAlgorithmCbIndex(tokens->tokenAlgorithm(row), token->algorithm());
            break;
        case OTPToken::Steam:
            tokens->tokenEditType(row)->setCurrentIndex(2);
            tokens->tokenEditSecretComboBoxExtra(row)->setCurrentIndex(1);
            tokens->tokenPeriod(row)->setText(QString::number(token->period()));
            break;
        case OTPToken::Authy:
            tokens->tokenEditType(row)->setCurrentIndex(3);
            tokens->tokenDigits(row)->setText(QString::number(token->digits()));
            tokens->tokenPeriod(row)->setText(QString::number(token->period()));
            break;
        case OTPToken::None: break;
    }

    tokens->tokenEditLabel(row)->setText(QString::fromUtf8(token->label().c_str()));
    setTokenIcon(row, token->icon());
    tokens->tokenEditSecret(row)->setText(QString::fromUtf8(token->secret().c_str()));
}

void TokenEditor::saveTokens()
{
    auto tokens = tokenEditWidget->tokens();

    QStringList skipped;

    auto override = _mode == OTPWidget::Mode::Override;

    for (auto i = 0; i < tokens->rowCount(); i++)
    {
        const auto type = static_cast<OTPToken::TokenType>(tokens->tokenEditType(i)->currentData().toUInt());
        const auto label = std::string(tokens->tokenEditLabel(i)->text().toUtf8().constData());

        if (_mode == OTPWidget::Mode::Edit)
        {
            if (TokenStore::i()->contains(label))
            {
                skipped << tokens->tokenEditLabel(i)->text();
                continue;
            }
        }

        std::string iconData;
        if (_mode == OTPWidget::Mode::Edit)
        {
            const auto iconUserData = tokens->tokenEditIcon(i)->userData(1);
            if (iconUserData)
            {
                const auto iconFile = static_cast<TokenIconUserData*>(iconUserData)->file;
                QFile file(iconFile);
                if (file.open(QIODevice::ReadOnly))
                {
                    auto buf = file.readAll();
                    iconData = std::string(buf.constData(), buf.constData() + buf.size());
                    buf.clear();
                    file.close();
                }
            }
        }
        // Mode Override
        else
        {
            const auto iconUserData = static_cast<TokenIconUserData*>(tokens->tokenEditIcon(i)->userData(1));
            if (iconUserData && !iconUserData->file.isEmpty())
            {
                const auto iconFile = iconUserData->file;
                QFile file(iconFile);
                if (file.open(QIODevice::ReadOnly))
                {
                    auto buf = file.readAll();
                    iconData = std::string(buf.constData(), buf.constData() + buf.size());
                    buf.clear();
                    file.close();
                }
            }
            else if (tokens->tokenEditIcon(i)->icon().isNull())
            {
                iconData.clear();
            }
            else
            {
                iconData = iconUserData->data;
            }
        }

        std::string secret;

        if (type == OTPToken::Steam)
        {
            auto steamSecret = std::string(tokens->tokenEditSecret(i)->text().toUtf8().constData());
            auto steamSecretBase = tokens->tokenEditSecretComboBoxExtra(i)->currentData().toString();

            if (steamSecretBase == "base64")
            {
                secret = SteamToken::convertBase64Secret(steamSecret);
                if (secret.empty())
                {
                    // avoid empty secret for better error handling
                    secret = steamSecret;
                }
            }
            else
            {
                secret = steamSecret;
            }

            steamSecret.clear();
            steamSecretBase.clear();
        }
        else
        {
            secret = std::string(tokens->tokenEditSecret(i)->text().toUtf8().constData());
        }

        const auto digits = static_cast<OTPToken::DigitType>(tokens->tokenDigits(i)->text().toUShort());
        const auto period = tokens->tokenPeriod(i)->text().toUInt();
        const auto counter = tokens->tokenCounter(i)->text().toUInt();

        OTPToken::ShaAlgorithm algorithm = OTPToken::Invalid;
        if (type == OTPToken::TOTP || type == OTPToken::HOTP)
        {
            algorithm = static_cast<OTPToken::ShaAlgorithm>(tokens->tokenAlgorithm(i)->currentData().toUInt());
        }
        else if (type == OTPToken::Steam)
        {
            algorithm = OTPToken::SHA1;
        }
        else if (type == OTPToken::Authy)
        {
            algorithm = static_cast<OTPToken::ShaAlgorithm>(static_cast<TokenAlgorithmUserData*>(tokens->cellWidget(i, 6)->userData(0))->algorithm);
        }

        TokenStore::Status tokenStatus;

        if (_mode == OTPWidget::Mode::Override)
        {
            const auto oldname = static_cast<TokenOldNameUserData*>(tokens->cellWidget(i, 1)->userData(2))->oldname;
            if (oldname != label)
            {
                auto res = TokenStore::i()->renameToken(oldname, label);
                if (!res)
                {
                    skipped << tokens->tokenEditLabel(i)->text();
                    continue;
                }
            }
        }

        switch (type)
        {
            case OTPToken::TOTP:
                tokenStatus = TokenStore::i()->addToken(std::make_shared<TOTPToken>(TOTPToken(
                    label, iconData, secret, digits, period, counter, algorithm
                )), override);
                break;

            case OTPToken::HOTP:
                tokenStatus = TokenStore::i()->addToken(std::make_shared<HOTPToken>(HOTPToken(
                    label, iconData, secret, digits, period, counter, algorithm
                )), override);
                break;

            case OTPToken::Steam:
                tokenStatus = TokenStore::i()->addToken(std::make_shared<SteamToken>(SteamToken(
                    label, iconData, secret, digits, period, counter, algorithm
                )), override);
                break;

            case OTPToken::Authy:
                tokenStatus = TokenStore::i()->addToken(std::make_shared<AuthyToken>(AuthyToken(
                    label, iconData, secret, digits, period, counter, algorithm
                )), override);
                break;

            case OTPToken::None:
                tokenStatus = TokenStore::Nullptr;
                break;
        }

        if (tokenStatus != TokenStore::Success)
        {
            skipped << tokens->tokenEditLabel(i)->text();
        }

#ifdef OTPGEN_DEBUG
        std::cout << "TokenEditor: label -> " << label << std::endl;
        std::cout << "TokenEditor: secret -> " << secret << std::endl;
        std::cout << "TokenEditor: tokenStatus -> " << tokenStatus << std::endl;
        std::cout << "---" << std::endl;
#endif

        secret.clear();
    }

    if (!skipped.isEmpty())
    {
        QMessageBox::information(this, "Notice",
            QString("The following tokens couldn't be saved due to name conflicts or empty secret:\n\n") +
            QString(" - ") +
            skipped.join("\n - ") +
            QString("\n\nLabels must be unique and not empty! Secrets are mandatory and can not be empty!")
        );
    }

    auto status = TokenDatabase::saveTokens();
    if (status != TokenDatabase::Success)
    {
        QMessageBox::critical(this, "Error", QString(TokenDatabase::getErrorMessage(status).c_str()));
    }

    emit tokensSaved();

    this->close();
}

void TokenEditor::process_otpauthURI(const std::string &out, bool fromQr)
{
    otpauthURI uri(out);
    if (!uri.valid())
    {
        if (fromQr)
        {
            QMessageBox::critical(this, "QR Code Error",
                QString("The given QR Code doesn't seem to be a valid otpauth:// URI.\n\n"
                        "Decoded text: %1").arg(QString::fromUtf8(out.c_str())));
        }
        else
        {
            QMessageBox::critical(this, "Invalid URI",
                        "The given otpauth URI isn't valid.");
        }
        return;
    }

    if (uri.type() == otpauthURI::TOTP)
    {
        TOTPToken token(uri.label(), "", uri.secret(), uri.digitsNumber(), uri.periodNumber(), 0, OTPToken::Invalid);
        token.setAlgorithm(uri.algorithm());
        if (token.algorithm() == OTPToken::Invalid)
        {
            QMessageBox::critical(this, "Invalid Token Algorithm",
                "The algorithm from the otpauth URI is invalid!");
            return;
        }
        addNewToken(&token);
    }
    else if (uri.type() == otpauthURI::HOTP)
    {
        HOTPToken token(uri.label(), "", uri.secret(), uri.digitsNumber(), 0, uri.counterNumber(), OTPToken::Invalid);
        token.setAlgorithm(uri.algorithm());
        if (token.algorithm() == OTPToken::Invalid)
        {
            QMessageBox::critical(this, "Invalid Token Algorithm",
                "The algorithm from the otpauth URI is invalid!");
            return;
        }
        addNewToken(&token);
    }
    else
    {
        QMessageBox::critical(this, "Invalid otpauth URI",
            QString("The otpauth URI is either incomplete or lacks required information.\n\n"
                    "URI: %1").arg(QString::fromUtf8(out.c_str())));
        return;
    }
}

void TokenEditor::showImportTokensMenu()
{
    importMenu->popup(QCursor::pos());
}

void TokenEditor::showExportTokensMenu()
{
    exportMenu->popup(QCursor::pos());
}

void TokenEditor::updateRow(int row)
{
    auto tokens = tokenEditWidget->tokens();

    // receive token type enum from current row
    const auto typeCb = tokens->tokenEditType(row);
    const auto type = static_cast<OTPToken::TokenType>(typeCb->itemData(typeCb->currentIndex()).toUInt());

    // transfer secret to new input widget, convenience feature when importing from
    // external files and adjusting the token type (user preference)
    auto secret = tokens->tokenEditSecret(row)->text();

    // set token options
    switch (type)
    {
        case OTPToken::TOTP: {
            tokens->setCellWidget(row, 2, OTPWidget::make_secretInput());

            tokens->tokenDigits(row)->setEnabled(true);
            tokens->tokenDigits(row)->setText("6");
            tokens->tokenPeriod(row)->setEnabled(true);
            tokens->tokenPeriod(row)->setText("30");
            tokens->tokenCounter(row)->setEnabled(false);
            tokens->setCellWidget(row, 6, OTPWidget::make_algoCb());
            tokens->tokenAlgorithm(row)->setEnabled(true);
            tokens->tokenAlgorithm(row)->setCurrentIndex(0);
            }
            break;

        case OTPToken::HOTP: {
            tokens->setCellWidget(row, 2, OTPWidget::make_secretInput());

            tokens->tokenDigits(row)->setEnabled(true);
            tokens->tokenDigits(row)->setText("6");
            tokens->tokenPeriod(row)->setEnabled(false);
            tokens->tokenPeriod(row)->setText("");
            tokens->tokenCounter(row)->setEnabled(true);
            tokens->setCellWidget(row, 6, OTPWidget::make_algoCb());
            tokens->tokenAlgorithm(row)->setEnabled(true);
            tokens->tokenAlgorithm(row)->setCurrentIndex(0);
            }
            break;

        case OTPToken::Steam: {
            tokens->setCellWidget(row, 2, OTPWidget::make_steamInput());

            tokens->tokenDigits(row)->setEnabled(false);
            tokens->tokenDigits(row)->setText("5");
            tokens->tokenPeriod(row)->setEnabled(false);
            tokens->tokenPeriod(row)->setText("30");
            tokens->tokenCounter(row)->setEnabled(false);
            tokens->setCellWidget(row, 6, OTPWidget::make_algoForSteam());
            tokens->cellWidget(row, 6)->setEnabled(false);
            }
            break;

        case OTPToken::Authy: {
            tokens->setCellWidget(row, 2, OTPWidget::make_secretInput());

            tokens->tokenDigits(row)->setEnabled(true);
            tokens->tokenDigits(row)->setText("7");
            tokens->tokenPeriod(row)->setEnabled(true);
            tokens->tokenPeriod(row)->setText("10");
            tokens->tokenCounter(row)->setEnabled(false);
            tokens->setCellWidget(row, 6, OTPWidget::make_algoForAuthy());
            tokens->cellWidget(row, 6)->setEnabled(true);
            }
            break;

        case OTPToken::None:
            break;
    }

    tokens->tokenEditSecret(row)->setText(secret);

    secret.clear();
}

void TokenEditor::deleteRow(int row)
{
    auto tokens = tokenEditWidget->tokens();

    if (_mode == OTPWidget::Mode::Override)
    {
        const auto res = QMessageBox::question(this, "Delete Token",
            QString("Are you sure you want to delete <b>%1</b>?").arg(tokens->tokenEditLabel(row)->text()));
        if (res == QMessageBox::Yes)
        {
            const auto oldname = static_cast<TokenOldNameUserData*>(tokens->cellWidget(row, 1)->userData(2))->oldname;
            TokenStore::i()->removeToken(oldname);
            emit tokensSaved();
        }
        else
        {
            return;
        }
    }

    // delete row widgets
    for (auto i = 0; i < tokens->columnCount(); i++)
    {
        delete tokens->cellWidget(row, i);
    }

    // delete current row
    tokens->removeRow(row);

    // update row data
    for (auto i = 0; i < tokens->rowCount(); i++)
    {
        static_cast<TableWidgetCellUserData*>(tokens->tokenEditType(i)->userData(0))->row = i;
        static_cast<TableWidgetCellUserData*>(tokens->tokenDeleteButton(i)->userData(0))->row = i;
    }
}

void TokenEditor::setAlgorithmCbIndex(QComboBox *cb, const OTPToken::ShaAlgorithm &algo)
{
    switch (algo)
    {
        case OTPToken::SHA1:
            cb->setCurrentIndex(0);
            break;
        case OTPToken::SHA256:
            cb->setCurrentIndex(1);
            break;
        case OTPToken::SHA512:
            cb->setCurrentIndex(2);
            break;
        case OTPToken::Invalid:
            break;
    }
}

void TokenEditor::setTokenIcon(int row)
{
    // construct a name filter of all supported image formats
    // different platforms and Qt builds may support different formats
    static const auto &supportedFormats = QImageReader::supportedImageFormats();
    static const auto &nameFilters = ([&]{
        QString filters;
        for (auto&& format : supportedFormats)
        {
            if (format == "gif" || format == "xcf" || format == "kra")
                continue;

            filters.append("*." + format + ' ');
        }
        return filters;
    })();

    auto file = QFileDialog::getOpenFileName(this, "Open Icon", QString(),
        "Images (" + nameFilters + ")");
    if (file.isEmpty() || file.isNull())
    {
        return;
    }

    if (file.endsWith("gif", Qt::CaseInsensitive))
    {
        return;
    }

    tokenEditWidget->tokens()->tokenEditIcon(row)->setIcon(QIcon(file));
    tokenEditWidget->tokens()->tokenEditIcon(row)->setUserData(1, new TokenIconUserData(file));
}

void TokenEditor::setTokenIcon(int row, const std::string &data)
{
    QPixmap pixmap;
    const auto status = pixmap.loadFromData(reinterpret_cast<const unsigned char*>(data.data()), static_cast<uint>(data.size()));
    if (status)
    {
        tokenEditWidget->tokens()->tokenEditIcon(row)->setIcon(pixmap.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    tokenEditWidget->tokens()->tokenEditIcon(row)->setUserData(1, new TokenIconUserData(data));
}

void TokenEditor::removeTokenIcon(int row)
{
    tokenEditWidget->tokens()->tokenEditIcon(row)->setIcon(QIcon());
    tokenEditWidget->tokens()->tokenEditIcon(row)->setUserData(1, nullptr);
}
