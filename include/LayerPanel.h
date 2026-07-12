#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidgetItem>
#include <QMenu>
#include <QUuid>

class Layer;
class LayerManager;
class DrawingPrimitive;
class Command;

// Custom widget for individual primitive items within a layer
class PrimitiveItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PrimitiveItemWidget(DrawingPrimitive* primitive, QWidget* parent = nullptr);
    ~PrimitiveItemWidget();

    DrawingPrimitive* primitive() const { return m_primitive; }
    void updateFromPrimitive();

signals:
    void primitiveSelected(DrawingPrimitive* primitive);
    void primitiveVisibilityToggled(DrawingPrimitive* primitive, bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onVisibilityClicked();

private:
    void setupUI();
    QString getPrimitiveTypeName() const;
    QString getPrimitiveDescription() const;

    DrawingPrimitive* m_primitive;
    
    // UI components
    QHBoxLayout* m_layout;
    QToolButton* m_visibilityButton;
    QLabel* m_typeLabel;
    QLabel* m_descriptionLabel;
    
    bool m_isSelected;
};

// Custom widget for individual layer items
class LayerItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LayerItemWidget(Layer* layer, QWidget* parent = nullptr);
    ~LayerItemWidget();

    Layer* layer() const { return m_layer; }
    void updateFromLayer();
    
    bool isSelected() const { return m_isSelected; }
    void setSelected(bool selected);

    
    bool isExpanded() const { return m_isExpanded; }
    void setExpanded(bool expanded);

signals:
    void visibilityToggled(Layer* layer, bool visible);
    void lockToggled(Layer* layer, bool locked);
    void opacityChanged(Layer* layer, float opacity);
    void nameChanged(Layer* layer, const QString& newName);
    void layerSelected(Layer* layer);
    void blendModeChanged(Layer* layer, int blendMode);
    void expandToggled(Layer* layer, bool expanded);
    void primitiveVisibilityToggled(DrawingPrimitive* primitive, bool visible);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onVisibilityClicked();
    void onLockClicked();
    void onOpacityChanged(int value);
    void onNameEditFinished();
    void onBlendModeChanged(int index);
    void onExpandClicked();

private:
    void setupUI();
    void updateVisibilityIcon();
    void updateLockIcon();
    void updatePrimitivesList();

    Layer* m_layer;
    
    // UI components
    QHBoxLayout* m_layout;
    QToolButton* m_expandButton;
    QToolButton* m_visibilityButton;
    QToolButton* m_lockButton;
    QLabel* m_colorLabel;
    QLineEdit* m_nameEdit;
    QSlider* m_opacitySlider;
    QComboBox* m_blendModeCombo;
    QLabel* m_primitiveCountLabel;
    
    // Primitive widgets container
    QWidget* m_primitivesContainer;
    QVBoxLayout* m_primitivesLayout;
    
    bool m_isSelected;
    bool m_isExpanded;
};

class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(QWidget* parent = nullptr);
    ~LayerPanel();

    void setLayerManager(LayerManager* manager);
    LayerManager* layerManager() const { return m_layerManager; }

signals:
    void layerSelectionChanged(Layer* layer);
    void layerPropertyChanged(); // Signal to update canvas when layer properties change
    void commandRequested(Command* command);
    void documentModified(); // Non-undoable structural edits (e.g. layer duplicate)

public slots:
    void refresh();
    void selectLayer(Layer* layer);

private slots:
    void onAddLayer();
    void onDeleteLayer();
    void onDuplicateLayer();
    void onMoveLayerUp();
    void onMoveLayerDown();
    void onShowAllLayers();
    void onHideAllLayers();
    void onPrimitiveVisibilityToggled(DrawingPrimitive* primitive, bool visible);
    
    // LayerManager signal handlers
    void onLayerCreated(Layer* layer);
    void onLayerDeleted(const QUuid& layerId);
    void onActiveLayerChanged(Layer* layer);
    void onLayersReordered();
    void onPrimitiveAdded(Layer* layer, DrawingPrimitive* primitive);
    
    // LayerItemWidget signal handlers
    void onLayerVisibilityToggled(Layer* layer, bool visible);
    void onLayerLockToggled(Layer* layer, bool locked);
    void onLayerOpacityChanged(Layer* layer, float opacity);
    void onLayerNameChanged(Layer* layer, const QString& newName);
    void onLayerSelected(Layer* layer);
    void onLayerBlendModeChanged(Layer* layer, int blendMode);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void setupUI();
    void setupToolbar();
    void connectSignals();
    void rebuildLayerList();
    LayerItemWidget* findLayerWidget(Layer* layer) const;
    Layer* getSelectedLayer() const;

    LayerManager* m_layerManager;
    
    // UI components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    
    // Toolbar buttons
    QPushButton* m_addLayerButton;
    QPushButton* m_deleteLayerButton;
    QPushButton* m_duplicateLayerButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    QPushButton* m_showAllButton;
    QPushButton* m_hideAllButton;
    
    // Layer list
    QScrollArea* m_scrollArea;
    QWidget* m_layerListWidget;
    QVBoxLayout* m_layerListLayout;
    
    // Context menu
    QMenu* m_contextMenu;
    
    // Current selection
    Layer* m_selectedLayer;
};
