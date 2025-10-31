package com.unina.libreria25.controller;

import com.unina.libreria25.model.Libro;
import com.unina.libreria25.service.NetworkService;

import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.concurrent.Task;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.stage.Stage;

public class LoginController {

    @FXML
    private TextField usernameField;
    @FXML
    private PasswordField passwordField;
    @FXML
    private Button loginBtn;
    @FXML
    private Button registerBtn;
    @FXML
    private Label statusLabel;
    @FXML
    private ProgressIndicator progressIndicator;

    private NetworkService networkService;
    private Stage stage;

    public void initData(Stage stage, NetworkService networkService) {
        this.stage = stage;
        this.networkService = networkService;
    }

    @FXML
    private void handleLogin() {
        String user = usernameField.getText();
        String pass = passwordField.getText();
        if (user.isEmpty() || pass.isEmpty()) {
            statusLabel.setText("Username e password non possono essere vuoti.");
            return;
        }

        Task<String> loginTask = new Task<>() {
            @Override
            protected String call() throws Exception {
                return networkService.login(user, pass);
            }
        };

        loginTask.setOnSucceeded(e -> {
            toggleControls(false);
            String response = loginTask.getValue();

            if ("LOGIN_OK".equalsIgnoreCase(response)) {
                // utente normale: prima fetch del limite, poi vai alla main view
                caricaMainView(user);
            } else if ("LOGIN_LIBRARIAN".equalsIgnoreCase(response)) {
                // libraio: stessa cosa
                caricaLibrarianView(user);
            } else {
                statusLabel.setText("Login fallito! Credenziali errate.");
            }
        });

        loginTask.setOnFailed(e -> {
            toggleControls(false);
            statusLabel.setText("Errore di connessione con il server.");
        });

        toggleControls(true);
        new Thread(loginTask).start();
    }

    @FXML
    private void handleRegister() {
        String user = usernameField.getText();
        String pass = passwordField.getText();
        if (user.isEmpty() || pass.isEmpty()) {
            statusLabel.setText("Username e password non possono essere vuoti.");
            return;
        }

        Task<String> registerTask = new Task<>() {
            @Override
            protected String call() throws Exception {
                return networkService.register(user, pass);
            }
        };

        registerTask.setOnSucceeded(e -> {
            toggleControls(false);
            String response = registerTask.getValue();
            if ("REGISTER_OK".equalsIgnoreCase(response)) {
                statusLabel.setText("Registrazione avvenuta con successo!");
            } else {
                statusLabel.setText("Registrazione fallita! Utente già esistente?");
            }
        });

        registerTask.setOnFailed(e -> {
            toggleControls(false);
            statusLabel.setText("Errore di connessione con il server.");
        });

        toggleControls(true);
        new Thread(registerTask).start();
    }

    /** ✅ Carica la vista per l'utente normale */
    private void caricaMainView(String username) {
        Task<ObservableList<Libro>> booksTask = new Task<>() {
            @Override
            protected ObservableList<Libro> call() throws Exception {
                return networkService.getBooks();
            }
        };

        booksTask.setOnSucceeded(ev -> {
            ObservableList<Libro> libri = booksTask.getValue();
            try {
                FXMLLoader loader = new FXMLLoader(getClass().getResource("../view/MainView.fxml"));
                Parent root = loader.load();

                Scene mainScene = new Scene(root);
                MainViewController controller = loader.getController();
                controller.initData(stage, mainScene, networkService, username, libri);

                stage.setScene(mainScene);
                stage.setTitle("Libreria - Utente");
                stage.centerOnScreen();
            } catch (Exception ex) {
                ex.printStackTrace();
                new Alert(Alert.AlertType.ERROR, "Impossibile caricare la vista principale.").show();
            }
        });

        booksTask
                .setOnFailed(ev -> new Alert(Alert.AlertType.ERROR, "Errore durante il caricamento dei libri.").show());

        new Thread(booksTask).start();
    }

    /** ✅ Carica la vista per il LIBRAIO */
    private void caricaLibrarianView(String username) {
        Task<ObservableList<Libro>> libriTask = new Task<>() {
            @Override
            protected ObservableList<Libro> call() throws Exception {
                return networkService.getBooks(); // opzionale
            }
        };

        libriTask.setOnSucceeded(ev -> {
            try {
                FXMLLoader loader = new FXMLLoader(
                        getClass().getResource("/com/unina/libreria25/view/LibrarianView.fxml"));
                Parent root = loader.load();

                Scene librarianScene = new Scene(root);
                LibrarianViewController controller = loader.getController();
                controller.initData(stage, librarianScene, networkService, username,
                        FXCollections.observableArrayList());

                stage.setScene(librarianScene);
                stage.setTitle("Libreria - Libraio");
                stage.centerOnScreen();

            } catch (Exception ex) {
                ex.printStackTrace();
                new Alert(Alert.AlertType.ERROR,
                        "Impossibile caricare la vista del libraio:\n" + ex.getMessage()).show();
            }
        });

        libriTask.setOnFailed(
                ev -> new Alert(Alert.AlertType.ERROR, "Errore durante il caricamento dei dati iniziali del libraio.")
                        .show());

        new Thread(libriTask).start();
    }

    private void toggleControls(boolean busy) {
        progressIndicator.setVisible(busy);
        loginBtn.setDisable(busy);
        registerBtn.setDisable(busy);
    }
}
