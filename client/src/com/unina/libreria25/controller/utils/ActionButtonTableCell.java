// File: src/com/unina/libreria25/controller/utils/ActionButtonTableCell.java
package com.unina.libreria25.controller.utils;

import java.util.function.Consumer;
import javafx.scene.control.Button;
import javafx.scene.control.TableCell;

public class ActionButtonTableCell<T> extends TableCell<T, Void> {
    private final Button actionButton;

    public ActionButtonTableCell(String buttonText, Consumer<T> action) {
        this.actionButton = new Button(buttonText);
        this.actionButton.setOnAction(event -> {
            // actionButton.setDisable(true);
            action.accept(getTableView().getItems().get(getIndex()));
        });
    }

    @Override
    protected void updateItem(Void item, boolean empty) {
        super.updateItem(item, empty);
        if (empty) {
            setGraphic(null);
        } else {
            setGraphic(actionButton);
            actionButton.setDisable(false);
        }
    }
}