package com.unina.libreria25.controller;

import com.unina.libreria25.model.Messaggio;
import com.unina.libreria25.service.NetworkService;
import javafx.collections.ObservableList;
import javafx.concurrent.Task;
import javafx.fxml.FXML;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.stage.Stage;

public class MessageViewController {

    @FXML private TableView<Messaggio> msgTable;
    @FXML private TableColumn<Messaggio, String> colData;
    @FXML private TableColumn<Messaggio, String> colTesto;
    @FXML private TableColumn<Messaggio, Void> colAzione;

    private Stage stage;
    private Scene mainScene;
    private NetworkService networkService;
    private String username;

    public void initData(Stage stage, Scene mainScene, NetworkService networkService, String username,
                         ObservableList<Messaggio> messaggi) {
        this.stage = stage;
        this.mainScene = mainScene;
        this.networkService = networkService;
        this.username = username;
        msgTable.setItems(messaggi);
    }

    @FXML
    private void initialize() {
        colData.setCellValueFactory(new javafx.scene.control.cell.PropertyValueFactory<>("data"));
        colTesto.setCellValueFactory(new javafx.scene.control.cell.PropertyValueFactory<>("testo"));

        colAzione.setCellFactory(param -> new TableCell<>() {
            private final Button btn = new Button("🗑 Elimina");
            {
                btn.setOnAction(e -> {
                    Messaggio m = getTableView().getItems().get(getIndex());
                    eliminaMessaggio(m);
                });
                btn.setStyle("-fx-font-size: 11px; -fx-padding: 3 8 3 8;");
            }
            @Override
            protected void updateItem(Void item, boolean empty) {
                super.updateItem(item, empty);
                setGraphic(empty ? null : btn);
            }
        });
    }

    private void eliminaMessaggio(Messaggio m) {
        Alert confirm = new Alert(Alert.AlertType.CONFIRMATION, "Eliminare questo messaggio?", ButtonType.YES, ButtonType.NO);
        confirm.showAndWait().ifPresent(res -> {
            if (res == ButtonType.YES) {
                Task<String> deleteTask = new Task<>() {
                    @Override
                    protected String call() throws Exception {
                        return networkService.deleteMessage(m.getCodMsg());
                    }
                };

                deleteTask.setOnSucceeded(e -> {
                    if ("DELETE_MSG_OK".equalsIgnoreCase(deleteTask.getValue())) {
                        msgTable.getItems().remove(m);
                    } else {
                        new Alert(Alert.AlertType.ERROR, "Errore durante l'eliminazione.").show();
                    }
                });

                deleteTask.setOnFailed(e -> new Alert(Alert.AlertType.ERROR, "Errore di comunicazione col server.").show());
                new Thread(deleteTask).start();
            }
        });
    }

    @FXML
    private void handleBack() {
        stage.setScene(mainScene);
    }
}
