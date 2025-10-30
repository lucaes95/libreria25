package com.unina.libreria25;

import com.unina.libreria25.controller.LoginController;
import com.unina.libreria25.service.NetworkService;

import javafx.application.Application;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.Alert;
import javafx.stage.Stage;

public class MainApp extends Application {

    private NetworkService networkService;

    @Override
    public void start(Stage primaryStage) throws Exception {
        networkService = new NetworkService();
        try {
            networkService.connect("localhost", 12345);
        } catch (Exception e) {
            new Alert(Alert.AlertType.ERROR, "Impossibile connettersi al server. L'applicazione verrà chiusa.").showAndWait();
            return;
        }

        FXMLLoader loader = new FXMLLoader(getClass().getResource("view/LoginView.fxml"));
        Parent root = loader.load();

        LoginController controller = loader.getController();
        controller.initData(primaryStage, networkService);

        primaryStage.setTitle("Libreria Client");
        primaryStage.setScene(new Scene(root));
        primaryStage.setOnCloseRequest(e -> networkService.close());
        primaryStage.show();
    }

    public static void main(String[] args) {
        launch(args);
    }
}