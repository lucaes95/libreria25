package com.unina.libreria25.controller;

import com.unina.libreria25.model.Prestito;
import com.unina.libreria25.service.NetworkService;
import javafx.collections.ObservableList;
import javafx.concurrent.Task;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.control.cell.PropertyValueFactory;
import javafx.stage.Stage;

public class LibrarianViewController {

    @FXML private Label welcomeLabel;
    @FXML private TableView<Prestito> prestitiTable;

    // Colonne
    @FXML private TableColumn<Prestito, String> colUtente;
    @FXML private TableColumn<Prestito, String> colTitolo;
    @FXML private TableColumn<Prestito, String> colScadenza;
    @FXML private TableColumn<Prestito, Integer> colCopieDisponibili;    
    @FXML private TableColumn<Prestito, Integer> colCopieInPrestito;
    @FXML private TableColumn<Prestito, Void> colAzione;

    private Stage stage;
    private Scene mainScene;
    private NetworkService networkService;
    private String username;

    // ✅ Metodo principale di inizializzazione
    public void initData(Stage stage, Scene mainScene, NetworkService networkService, String username, ObservableList<Prestito> unused) {
        this.stage = stage;
        this.mainScene = mainScene;
        this.networkService = networkService;
        this.username = username;

        welcomeLabel.setText("Benvenuto, " + username + " (Libraio)");

        // Carica i dati dei prestiti dal server
        Task<ObservableList<Prestito>> loadPrestitiTask = new Task<>() {
            @Override
            protected ObservableList<Prestito> call() throws Exception {
                return networkService.getAllPrestiti();
            }
        };

        loadPrestitiTask.setOnSucceeded(e -> prestitiTable.setItems(loadPrestitiTask.getValue()));
        loadPrestitiTask.setOnFailed(e -> 
            new Alert(Alert.AlertType.ERROR, "Errore nel caricamento dei prestiti dal server.").show()
        );

        new Thread(loadPrestitiTask).start();
    }

    // ✅ Logout
    @FXML
    private void handleLogout() {
        try {
            FXMLLoader loader = new FXMLLoader(getClass().getResource("../view/LoginView.fxml"));
            Parent root = loader.load();
            LoginController controller = loader.getController();
            NetworkService newService = new NetworkService();
            newService.connect("localhost", 12345);
            controller.initData(stage, newService);
            stage.setScene(new Scene(root));
            stage.centerOnScreen();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    // ✅ Setup colonne
    @FXML
    private void initialize() {
        colUtente.setCellValueFactory(new PropertyValueFactory<>("utente"));
        colTitolo.setCellValueFactory(new PropertyValueFactory<>("titolo"));
        colScadenza.setCellValueFactory(new PropertyValueFactory<>("dataFine"));
        colCopieDisponibili.setCellValueFactory(new PropertyValueFactory<>("copieDisponibili"));
        colCopieInPrestito.setCellValueFactory(new PropertyValueFactory<>("copieInPrestito"));

        setupAzioneButtons();
    }

    // ✅ Colonna "Contatta"
private void setupAzioneButtons() {
    colAzione.setCellFactory(param -> new TableCell<>() {
        private final Button btn = new Button("Contatta");

        {
            btn.setStyle("-fx-font-size: 11px; -fx-padding: 2 6 2 6;");
            btn.setOnAction(e -> {
                Prestito prestito = getTableView().getItems().get(getIndex());
                mostraFinestraMessaggio(prestito.getUtente(), prestito.getCodPrestito());
            });
        }

        @Override
        protected void updateItem(Void item, boolean empty) {
            super.updateItem(item, empty);

            if (empty) {
                setGraphic(null);
                return;
            }

            Prestito prestito = getTableView().getItems().get(getIndex());
            boolean isScaduto = false;

            // 🔍 Controllo data di scadenza
            try {
                java.time.LocalDate dataFine = java.time.LocalDate.parse(
                        prestito.getDataFine(),
                        java.time.format.DateTimeFormatter.ofPattern("dd/MM/yyyy")
                );
                isScaduto = dataFine.isBefore(java.time.LocalDate.now());
            } catch (Exception ignored) {}

            // ✅ Mostra bottone solo se scaduto
            setGraphic(isScaduto ? btn : null);
        }
    });
}


    // ✅ Finestra di invio messaggio
    private void mostraFinestraMessaggio(String utente, int codPrestito) {
        TextInputDialog dialog = new TextInputDialog();
        dialog.setTitle("Invia messaggio");
        dialog.setHeaderText("Invia un messaggio a " + utente);
        dialog.setContentText("Messaggio (max 150 caratteri):");

        dialog.showAndWait().ifPresent(msg -> {
            if (msg.length() > 150) {
                new Alert(Alert.AlertType.WARNING, "Messaggio troppo lungo (max 150 caratteri).").show();
                return;
            }

            Task<String> sendTask = new Task<>() {
                @Override
                protected String call() throws Exception {
                    return networkService.sendMessage(utente, codPrestito, msg);
                }
            };

            sendTask.setOnSucceeded(ev -> {
                String resp = sendTask.getValue();
                if ("SEND_MSG_OK".equalsIgnoreCase(resp))
                    new Alert(Alert.AlertType.INFORMATION, "Messaggio inviato.").show();
                else
                    new Alert(Alert.AlertType.ERROR, "Errore durante l'invio: " + resp).show();
            });

            sendTask.setOnFailed(ev ->
                new Alert(Alert.AlertType.ERROR, "Errore di comunicazione col server.").show()
            );

            new Thread(sendTask).start();
        });
    }
}
