package com.unina.libreria25.controller;

import com.unina.libreria25.model.Prestito;
import com.unina.libreria25.service.NetworkService;
import javafx.collections.ObservableList;
import javafx.concurrent.Task;
import javafx.event.ActionEvent;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.control.cell.PropertyValueFactory;
import javafx.stage.Stage;

public class LibrarianViewController {

    @FXML
    private Label welcomeLabel;
    @FXML
    private TableView<Prestito> prestitiTable;

    // Colonne
    @FXML
    private TableColumn<Prestito, String> colUtente;
    @FXML
    private TableColumn<Prestito, String> colTitolo;
    @FXML
    private TableColumn<Prestito, String> colScadenza;
    @FXML
    private TableColumn<Prestito, Integer> colCopieDisponibili;
    @FXML
    private TableColumn<Prestito, Integer> colCopieInPrestito;
    @FXML
    private TableColumn<Prestito, Void> colAzione;

    @FXML
    private TextField txtMaxLoans;
    @FXML
    private Button btnReadMaxLoans;
    @FXML
    private Button btnSaveMaxLoans;
    @FXML
    private Label lblMaxLoansStatus;

    private Stage stage;
    private Scene mainScene;
    private NetworkService networkService;
    private String username;

    // ✅ Metodo principale di inizializzazione
    public void initData(Stage stage, Scene mainScene, NetworkService networkService, String username,
            ObservableList<Prestito> unused) {
        this.stage = stage;
        this.mainScene = mainScene;
        this.networkService = networkService;
        this.username = username;

        welcomeLabel.setText("Benvenuto, " + username + " (Libraio)");

        // GET subito appena ho il service
        triggerReadMaxLoans();

        // Carica i dati dei prestiti dal server
        Task<ObservableList<Prestito>> loadPrestitiTask = new Task<>() {
            @Override
            protected ObservableList<Prestito> call() throws Exception {
                return networkService.getAllPrestiti();
            }
        };

        loadPrestitiTask.setOnSucceeded(e -> prestitiTable.setItems(loadPrestitiTask.getValue()));
        loadPrestitiTask.setOnFailed(
                e -> new Alert(Alert.AlertType.ERROR, "Errore nel caricamento dei prestiti dal server.").show());

        new Thread(loadPrestitiTask).start();
    }

    @FXML
    private void onMaxLoansEnter(ActionEvent e) {
        // Invio con ENTER: comportati come "Salva"
        onSaveMaxLoans();
    }

    @FXML
    private void onReadMaxLoans() { // collegalo al bottone "Leggi"
        triggerReadMaxLoans();
    }

    private void triggerReadMaxLoans() {
        if (networkService == null)
            return;

        // 1) Prova cache immediata (UI reattiva)
        int cached = networkService.getCachedMaxLoans();
        if (cached > 0) {
            txtMaxLoans.setText(String.valueOf(cached));
            lblMaxLoansStatus.setText("Valore attuale: " + cached);
            // (opzionale) fare anche un refresh silenzioso in background:
            Task<Integer> refresh = new Task<>() {
                @Override
                protected Integer call() throws Exception {
                    return networkService.fetchMaxLoans();
                }
            };
            refresh.setOnSucceeded(ev -> {
                int val = refresh.getValue();
                if (val != cached) { // aggiorna solo se è cambiato
                    txtMaxLoans.setText(String.valueOf(val));
                    lblMaxLoansStatus.setText("Valore aggiornato: " + val);
                }
            });
            // refresh.setOnFailed(ev -> { /* ignora o logga */ });
            new Thread(refresh).start();
            return;
        }

        // 2) Cache fredda: fai GET in background e mostra "in corso…"
        lblMaxLoansStatus.setText("Lettura in corso…");
        Task<Integer> t = new Task<>() {
            @Override
            protected Integer call() throws Exception {
                return networkService.getOrFetchMaxLoans(); // userà fetch perché cache è -1
            }
        };
        t.setOnSucceeded(e -> {
            int val = t.getValue();
            txtMaxLoans.setText(String.valueOf(val));
            lblMaxLoansStatus.setText("Valore attuale: " + val);
        });
        t.setOnFailed(e -> {
            txtMaxLoans.clear();
            lblMaxLoansStatus.setText("Errore: impossibile leggere il valore");
        });
        new Thread(t).start();
    }

    @FXML
    private void onSaveMaxLoans() { // collegalo al bottone "Salva"
        if (networkService == null)
            return;
        String raw = txtMaxLoans.getText() != null ? txtMaxLoans.getText().trim() : "";
        int newVal;
        try {
            newVal = Integer.parseInt(raw);
            if (newVal <= 0 || newVal > 20) { // stesso range che hai messo lato server
                lblMaxLoansStatus.setText("Inserisci un intero tra 1 e 20");
                return;
            }
        } catch (NumberFormatException ex) {
            lblMaxLoansStatus.setText("Inserisci un intero valido");
            return;
        }

        lblMaxLoansStatus.setText("Salvataggio in corso…");
        Task<Integer> t = new Task<>() {
            @Override
            protected Integer call() throws Exception {
                // SET sul server, poi rileggi il valore reale scritto
                return networkService.setMaxLoansAndGet(newVal);
            }
        };
        t.setOnSucceeded(e -> {
            int updated = t.getValue();
            txtMaxLoans.setText(String.valueOf(updated));
            lblMaxLoansStatus.setText("Salvato: " + updated);
        });
        t.setOnFailed(e -> {
            Throwable ex = t.getException();
            String msg = (ex != null && ex.getMessage() != null) ? ex.getMessage() : "Errore durante il salvataggio";
            lblMaxLoansStatus.setText(msg);
        });
        new Thread(t).start();
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

        // GET iniziale (se networkService non è ancora settato, lo rifacciamo in
        // initData)
        if (networkService != null) {
            triggerReadMaxLoans();
        }
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
                            java.time.format.DateTimeFormatter.ofPattern("dd/MM/yyyy"));
                    isScaduto = dataFine.isBefore(java.time.LocalDate.now());
                } catch (Exception ignored) {
                }

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

            sendTask.setOnFailed(ev -> new Alert(Alert.AlertType.ERROR, "Errore di comunicazione col server.").show());

            new Thread(sendTask).start();
        });
    }
}
