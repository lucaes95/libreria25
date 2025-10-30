package com.unina.libreria25.controller;

import java.util.Arrays;

import com.unina.libreria25.controller.utils.ActionButtonTableCell;
import com.unina.libreria25.model.Libro;
import com.unina.libreria25.model.Messaggio;
import com.unina.libreria25.model.Prestito;
import com.unina.libreria25.service.NetworkService;
import com.unina.libreria25.support.ServerResponses;

import javafx.application.Platform;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.concurrent.Task;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.control.cell.PropertyValueFactory;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

public class MainViewController {

    @FXML
    private Label welcomeLabel;
    @FXML
    private TableView<Libro> libriTable;
    @FXML
    private TableColumn<Libro, Integer> colCod;
    @FXML
    private TableColumn<Libro, String> colTitolo;
    @FXML
    private TableColumn<Libro, String> colAutore;
    @FXML
    private TableColumn<Libro, Integer> colCopie;
    @FXML
    private TableColumn<Libro, Void> colAzione;

    @FXML
    private TableView<Libro> cartTable;
    @FXML
    private TableColumn<Libro, String> colCartTitolo;
    @FXML
    private TableColumn<Libro, Void> colCartRimuovi;

    private Stage stage;
    private NetworkService networkService;
    private String username;
    private ObservableList<Libro> cartItems = FXCollections.observableArrayList();
    private Scene mainScene;
    // segnala che, rientrando nella main, dobbiamo aggiornare libri/carrello
    private final java.util.concurrent.atomic.AtomicBoolean booksDirty = new java.util.concurrent.atomic.AtomicBoolean(
            false);

    public void initData(Stage stage, Scene mainScene, NetworkService networkService, String username,
            ObservableList<Libro> libri) {
        this.stage = stage;
        this.mainScene = mainScene;
        this.networkService = networkService;
        this.username = username;
        welcomeLabel.setText("Benvenuto, " + username + "!");
        libriTable.setItems(libri);

    }

    @FXML
    private void initialize() {
        colCod.setCellValueFactory(new PropertyValueFactory<>("codLibro"));
        colTitolo.setCellValueFactory(new PropertyValueFactory<>("titolo"));
        colAutore.setCellValueFactory(new PropertyValueFactory<>("autore"));
        colCopie.setCellValueFactory(new PropertyValueFactory<>("copie"));
        colCartTitolo.setCellValueFactory(new PropertyValueFactory<>("titolo"));

        setupAzioneButtons();
        setupRimuoviButtons();

        cartTable.setItems(cartItems);
    }

    @FXML
    private void handleLogout() {
        try {
            // 👇 chiudi prima il vecchio servizio
            if (networkService != null) {
                networkService.close();
            }
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

    @FXML
    private void handleCheckout() {
        if (cartItems.isEmpty()) {
            new Alert(Alert.AlertType.WARNING, "Il carrello è vuoto.").show();
            return;
        }

        Task<String> checkoutTask = new Task<>() {
            @Override
            protected String call() throws Exception {
                return networkService.checkout();
            }
        };

        checkoutTask.setOnSucceeded(e -> {
            String response = checkoutTask.getValue();

            // 1. Definisci il task di aggiornamento QUI, fuori dall'if
            // Questo task risolverà il carrello "pagato"
            Task<Void> updateAllTask = new Task<>() {
                private ObservableList<Libro> nuoviLibri;
                private ObservableList<Integer> codiciCart;

                @Override
                protected Void call() throws Exception {
                    // Chiede al server il carrello (che è vuoto)
                    codiciCart = networkService.getCartCodici();
                    // Chiede la lista libri (che non è cambiata)
                    nuoviLibri = networkService.getBooks();
                    return null;
                }

                @Override
                protected void succeeded() {
                    // Sincronizza il carrello UI (lo svuota)
                    ObservableList<Libro> nuoviCartItems = FXCollections.observableArrayList();
                    for (Libro l : nuoviLibri) {
                        if (codiciCart.contains(l.getCodLibro())) {
                            nuoviCartItems.add(l);
                        }
                    }
                    cartItems.setAll(nuoviCartItems);
                    cartTable.refresh();

                    // Sincronizza la lista libri
                    libriTable.setItems(nuoviLibri);
                }

                @Override
                protected void failed() {
                    new Alert(Alert.AlertType.ERROR, "Errore durante l'aggiornamento post-checkout.").show();
                }
            };

            // 2. Mostra l'alert appropriato in base alla risposta
            if (ServerResponses.CHECKOUT_OK.equalsIgnoreCase(response)) {
                new Alert(Alert.AlertType.INFORMATION, "Prestito registrato con successo!").show();

            } else if (ServerResponses.CHECKOUT_FAIL_LIMIT.equalsIgnoreCase(response)) {
                new Alert(Alert.AlertType.WARNING,
                        "Operazione fallita: Hai raggiunto il limite massimo di 3 prestiti contemporanei.").show();

            } else if ("CHECKOUT_FAIL_NOT_AVAILABLE".equalsIgnoreCase(response)) {
                new Alert(Alert.AlertType.WARNING,
                        "Uno o più libri del carrello non sono più disponibili.").show();

            } else {
                new Alert(Alert.AlertType.ERROR, "Errore durante il prestito: " + response).show();
            }

            new Thread(updateAllTask).start(); // Aggiornamento UI sempre

        });

        checkoutTask.setOnFailed(e -> showCommunicationErrorAlert());
        new Thread(checkoutTask).start();
    }

    @FXML
    private void handleShowPrestiti() {
        Task<ObservableList<Prestito>> prestitiTask = new Task<>() {
            @Override
            protected ObservableList<Prestito> call() throws Exception {
                return networkService.getPrestiti(username);
            }
        };

        prestitiTask.setOnSucceeded(e -> showPrestitiScene(prestitiTask.getValue()));
        prestitiTask.setOnFailed(e -> new Alert(Alert.AlertType.ERROR, "Impossibile recuperare i prestiti.").show());
        new Thread(prestitiTask).start();
    }

    private void setupAzioneButtons() {
        colAzione.setCellFactory(param -> new ActionButtonTableCell<>("🛒 Aggiungi", libro -> {
            Task<String> cartTask = new Task<>() {
                @Override
                protected String call() throws Exception {
                    return networkService.addToCart(libro.getCodLibro());
                }
            };

            cartTask.setOnSucceeded(e -> {
                String response = cartTask.getValue();
                Platform.runLater(() -> {
                    if ("CART_ADDED".equalsIgnoreCase(response)) {
                        if (!cartItems.contains(libro)) {
                            cartItems.add(libro);
                        }

                    } else if ("CART_ALREADY".equalsIgnoreCase(response)) {
                        new Alert(Alert.AlertType.INFORMATION, "Libro già presente nel carrello.").show();
                        refreshCartFromServer();

                    } else if ("CART_FULL".equalsIgnoreCase(response)) {
                        new Alert(Alert.AlertType.WARNING, "Carrello pieno (max 3 libri).").show();

                    } else if ("NOT_AVAILABLE".equalsIgnoreCase(response)) {
                        // ⚠️ Nuovo caso: conflitto sull’ultima copia
                        new Alert(Alert.AlertType.WARNING,
                                "L'ultima copia del libro non è più disponibile.").show();

                        // Aggiorna sia la lista dei libri che il carrello
                        refreshMainBookList();
                        refreshCartFromServer();

                    } else if ("CART_FAIL".equalsIgnoreCase(response)) {
                        new Alert(Alert.AlertType.ERROR, "Errore durante l'aggiunta al carrello.").show();
                        refreshCartFromServer();

                    } else {
                        new Alert(Alert.AlertType.ERROR, "Errore imprevisto: " + response).show();
                        refreshCartFromServer();
                    }
                });
            });

            cartTask.setOnFailed(e -> Platform.runLater(this::showCommunicationErrorAlert));
            new Thread(cartTask).start();
        }));
    }

    @FXML
    private void handleShowMessaggi() {
        Task<ObservableList<Messaggio>> loadMsgsTask = new Task<>() {
            @Override
            protected ObservableList<Messaggio> call() throws Exception {
                return networkService.getMessages(username);
            }
        };

        loadMsgsTask.setOnSucceeded(e -> {
            try {
                FXMLLoader loader = new FXMLLoader(getClass().getResource("../view/MessageView.fxml"));
                Parent root = loader.load();
                MessageViewController controller = loader.getController();
                controller.initData(stage, mainScene, networkService, username, loadMsgsTask.getValue());
                stage.setScene(new Scene(root));
            } catch (Exception ex) {
                ex.printStackTrace();
            }
        });

        loadMsgsTask.setOnFailed(e -> new Alert(Alert.AlertType.ERROR, "Errore nel caricamento dei messaggi.").show());

        new Thread(loadMsgsTask).start();
    }

    private void setupRimuoviButtons() {
        colCartRimuovi.setCellFactory(param -> new ActionButtonTableCell<>("❌", libro -> {
            Task<String> removeTask = new Task<>() {
                @Override
                protected String call() throws Exception {
                    return networkService.removeFromCart(libro.getCodLibro());
                }
            };

            removeTask.setOnSucceeded(e -> {
                String response = removeTask.getValue();
                Platform.runLater(() -> {
                    if (ServerResponses.REMOVE_OK.equalsIgnoreCase(response)) {
                        cartItems.remove(libro);
                    } else if (ServerResponses.REMOVE_NOT_FOUND.equalsIgnoreCase(response)) {
                        new Alert(Alert.AlertType.WARNING, "Elemento non più presente nel carrello.").show();
                        refreshCartFromServer();
                    } else {
                        new Alert(Alert.AlertType.ERROR, "Errore: " + response).show();
                        refreshCartFromServer();
                    }
                });
            });

            removeTask.setOnFailed(e -> Platform.runLater(this::showCommunicationErrorAlert));
            new Thread(removeTask).start();
        }));
    }

    private void showPrestitiScene(ObservableList<Prestito> prestiti) {
        TableView<Prestito> prestitiTable = new TableView<>(prestiti);

        TableColumn<Prestito, String> colTitoloP = new TableColumn<>("Titolo");
        colTitoloP.setCellValueFactory(new PropertyValueFactory<>("titolo"));
        TableColumn<Prestito, String> colScadenzaP = new TableColumn<>("Scadenza");
        colScadenzaP.setCellValueFactory(new PropertyValueFactory<>("dataFine"));
        TableColumn<Prestito, Void> colAzione = new TableColumn<>("Azione");

        colAzione.setCellFactory(param -> new ActionButtonTableCell<>("📚 Restituisci", prestito -> {
            Task<String> restituisciTask = new Task<>() {
                @Override
                protected String call() throws Exception {
                    return networkService.restituisciLibro(prestito.getCodLibro());
                }
            };

            // ****** INIZIO MODIFICA IMPORTANTE ******
            restituisciTask.setOnSucceeded(e -> {
                String resp = restituisciTask.getValue() != null ? restituisciTask.getValue().trim() : "";

                Platform.runLater(() -> {
                    if (ServerResponses.RETURN_OK.equalsIgnoreCase(resp)) {
                        new Alert(Alert.AlertType.INFORMATION, "Libro restituito con successo!").show();

                        // ✅ Aggiorna SOLO i prestiti in questa scena
                        Task<ObservableList<Prestito>> reloadPrestitiTask = new Task<>() {
                            @Override
                            protected ObservableList<Prestito> call() throws Exception {
                                return networkService.getPrestiti(username);
                            }
                        };
                        reloadPrestitiTask.setOnSucceeded(ev -> prestitiTable.setItems(reloadPrestitiTask.getValue()));
                        reloadPrestitiTask.setOnFailed(
                                ev -> new Alert(Alert.AlertType.ERROR, "Errore aggiornando i prestiti.").show());
                        new Thread(reloadPrestitiTask).start();

                        // ✅ Segna che i libri della main vanno aggiornati al rientro
                        booksDirty.set(true);

                    } else if (ServerResponses.RETURN_FAIL.equalsIgnoreCase(resp)) {
                        new Alert(Alert.AlertType.WARNING,
                                "Restituzione fallita: libro già restituito o inesistente.").show();
                        // Aggiorna comunque i prestiti per sicurezza
                        refreshPrestitiList(prestitiTable);
                    } else {
                        new Alert(Alert.AlertType.ERROR, "Errore nella restituzione: " + resp).show();
                    }

                });
            });
            // ****** FINE MODIFICA IMPORTANTE ******
            restituisciTask.setOnFailed(e -> Platform.runLater(this::showCommunicationErrorAlert));
            new Thread(restituisciTask).start();
        }));

        prestitiTable.getColumns().setAll(Arrays.asList(colTitoloP, colScadenzaP, colAzione));
        prestitiTable.setColumnResizePolicy(TableView.CONSTRAINED_RESIZE_POLICY_FLEX_LAST_COLUMN);

        Button backBtn = new Button("⬅ Torna indietro");
        VBox layout = new VBox(10, new Label("I tuoi prestiti:"), prestitiTable, backBtn);
        layout.setStyle("-fx-padding: 20;");

        Scene prestitiScene = new Scene(layout, 800, 500);
        backBtn.setOnAction(e -> {
            stage.setScene(mainScene);
            // 👇 aggiornamenti sequenziali solo al rientro
            if (booksDirty.getAndSet(false)) {
                refreshMainBookList();
                refreshCartFromServer();
            }
        });
        stage.setScene(prestitiScene);
    }

    // --- METODI DI SUPPORTO ---

    private void refreshPrestitiList(TableView<Prestito> targetTable) {
        Task<ObservableList<Prestito>> prestitiTask = new Task<>() {
            @Override
            protected ObservableList<Prestito> call() throws Exception {
                return networkService.getPrestiti(username);
            }
        };

        prestitiTask.setOnSucceeded(e -> Platform.runLater(() -> targetTable.setItems(prestitiTask.getValue())));

        prestitiTask.setOnFailed(e -> Platform.runLater(
                () -> new Alert(Alert.AlertType.ERROR, "Impossibile aggiornare la lista dei prestiti.").show()));

        new Thread(prestitiTask).start();
    }

    private void refreshMainBookList() {
        Task<ObservableList<Libro>> updateBooksTask = new Task<>() {
            @Override
            protected ObservableList<Libro> call() throws Exception {
                return networkService.getBooks();
            }
        };

        updateBooksTask.setOnSucceeded(evt -> Platform.runLater(() -> libriTable.setItems(updateBooksTask.getValue())));

        new Thread(updateBooksTask).start();
    }

    /** 🔄 Sincronizza il carrello con lo stato reale sul server */
    private void refreshCartFromServer() {
        Task<ObservableList<Integer>> cartCodesTask = new Task<>() {
            @Override
            protected ObservableList<Integer> call() throws Exception {
                return networkService.getCartCodici();
            }
        };

        cartCodesTask.setOnSucceeded(evt -> {
            ObservableList<Integer> codici = cartCodesTask.getValue();
            ObservableList<Libro> nuovi = FXCollections.observableArrayList();

            ObservableList<Libro> currentBooks = libriTable.getItems();
            if (currentBooks != null) {
                for (Libro l : currentBooks) {
                    if (codici.contains(l.getCodLibro())) {
                        nuovi.add(l);
                    }
                }
            }
            Platform.runLater(() -> {
                cartItems.setAll(nuovi);
                cartTable.refresh();
            });
        });

        cartCodesTask.setOnFailed(evt -> System.err.println("Errore durante l’aggiornamento del carrello."));
        new Thread(cartCodesTask).start();
    }

    private void showCommunicationErrorAlert() {
        new Alert(Alert.AlertType.ERROR, "Errore di comunicazione con il server.").show();
    }
}
