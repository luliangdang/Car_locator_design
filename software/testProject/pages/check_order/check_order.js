// pages/check_order/check_order.js
import { Config } from '../../utils/config.js';

Page({

  /**
   * 页面的初始数据
   */
  data: {
    order:{},
    idx:0
  },

  /**
   * 生命周期函数--监听页面加载
   */
  onLoad: function (options) {
    var that = this;
    wx.request({
      url: Config.restUrl+'/api/v1/user/1', // 仅为示例，并非真实的接口地址
      header: {
        'content-type': 'application/json' // 默认值
      },
      success(res) {
        console.log()
        var dataxx = res.data;
        console.log("赋值完成")
        that.setData2page(dataxx)
      }
    })
    console.log(this.data.order);
  },

  setData2page: function (data) {
    console.log("我正在工作");
    this.setData({
      order: data
    });
    console.log(this.data.order);
  },

  LIOrderDetail: function(event){
    console.log('我在这跳转啊啊啊啊啊啊啊啊');
    console.log(event);
    var oid = this.getDataSet(event, 'id');
    wx.navigateTo({
      url: '../now_order/now_order?oid=' + oid
    })
  },

  getDataSet(event, key) {
    return event.currentTarget.dataset[key];
  },

  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {

  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow: function () {

  },

  /**
   * 生命周期函数--监听页面隐藏
   */
  onHide: function () {

  },

  /**
   * 生命周期函数--监听页面卸载
   */
  onUnload: function () {

  },

  /**
   * 页面相关事件处理函数--监听用户下拉动作
   */
  onPullDownRefresh: function () {

  },

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {

  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})